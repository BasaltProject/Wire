#include <basalt/wire/multiversion/java/DefaultCodec.hpp>
#include <basalt/wire/multiversion/java/Link736Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kMultiBlockChange = 0x0F;
constexpr std::int32_t kMultiBlockChangeNew = 0x3B;
constexpr std::int32_t kChunk = 0x21;
constexpr std::int32_t kUnlockRecipes = 0x36;
constexpr std::int32_t kLogin = 0x25;
constexpr std::int32_t kRespawn = 0x3A;

constexpr std::int32_t kHardcoreBit = 0x08;
constexpr std::int32_t kBiomesInChunk = 1024;
constexpr std::string_view kOverworld = "minecraft:overworld";

/// parity: `RegistryAccess` of 1.16.2, which is what a 1.16.1 server never sent: biomes and dimension types.
[[nodiscard]] const nbt::NbtTag& codec() {
    static const nbt::NbtTag tag = [] {
        const auto parsed = nbt::nbtIO::readAnonymous(defaultCodec1_16_2());
        return parsed ? parsed->tag : nbt::NbtTag{};
    }();
    return tag;
}

[[nodiscard]] nbt::NbtTag dimensionElement(std::string_view name) {
    const nbt::NbtCompound* root = codec().get<nbt::NbtCompound>();
    const nbt::NbtCompound* types = root != nullptr ? root->getCompound("minecraft:dimension_type") : nullptr;
    const nbt::NbtList* values = types != nullptr ? types->getList("value") : nullptr;
    nbt::NbtTag fallback;
    for (std::size_t index = 0; values != nullptr && index < values->size(); ++index) {
        const nbt::NbtCompound* entry = (*values)[index].get<nbt::NbtCompound>();
        const nbt::NbtTag* element = entry != nullptr ? entry->find("element") : nullptr;
        if (element == nullptr) {
            continue;
        }
        if (entry->getString("name") == name) {
            return *element;
        }
        if (entry->getString("name") == kOverworld) {
            fallback = *element;
        }
    }
    return fallback;
}

} // namespace

bool Link736Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kLogin: {
        out.writeInt(in.readInt());
        const std::int32_t mode = in.readByte();
        const std::uint8_t previous = in.readByte();
        pv::writeBoolean(out, (mode & kHardcoreBit) != 0);
        out.writeByte(static_cast<std::uint8_t>(mode & ~kHardcoreBit));
        out.writeByte(previous);
        const std::int32_t worlds = pv::readCollectionSize(in, 1);
        pv::varInt::write(out, worlds);
        for (std::int32_t index = 0; index < worlds && !in.failed(); ++index) {
            pv::writeString(out, pv::readString(in));
        }
        static_cast<void>(readNamedNbt(in));
        const std::string dimension = pv::readString(in);
        writeNamedNbt(out, codec());
        writeNamedNbt(out, dimensionElement(dimension));
        pv::writeString(out, pv::readString(in));
        out.writeLong(in.readLong());
        pv::varInt::write(out, in.readByte());
        out.writeBytes(in.readable());
        break;
    }
    case kRespawn: {
        writeNamedNbt(out, dimensionElement(pv::readString(in)));
        out.writeBytes(in.readable());
        break;
    }
    case kChunk: {
        out.writeInt(in.readInt());
        out.writeInt(in.readInt());
        const bool full = pv::readBoolean(in);
        pv::writeBoolean(out, full);
        static_cast<void>(in.readByte());
        pv::varInt::write(out, pv::varInt::read(in));
        const auto heightmaps = readNamedNbt(in);
        if (in.failed()) {
            return false;
        }
        out.writeBytes(heightmaps->raw);
        if (full) {
            pv::varInt::write(out, kBiomesInChunk);
            for (std::int32_t index = 0; index < kBiomesInChunk && !in.failed(); ++index) {
                pv::varInt::write(out, in.readInt());
            }
        }
        out.writeBytes(in.readable());
        break;
    }
    case kMultiBlockChange: {
        const std::int32_t chunkX = in.readInt();
        const std::int32_t chunkZ = in.readInt();
        const std::int32_t count = pv::readCollectionSize(in, 1);
        std::map<std::int32_t, std::vector<std::int64_t>> sections;
        for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
            const std::uint8_t horizontal = in.readByte();
            const std::uint8_t y = in.readByte();
            const std::int64_t block = pv::varInt::read(in);
            const std::int64_t local = (static_cast<std::int64_t>(horizontal >> 4) << 8) |
                                       (static_cast<std::int64_t>(horizontal & 15) << 4) | (y & 15);
            sections[y >> 4].push_back((block << 12) | local);
        }
        if (in.failed() || sections.empty()) {
            return false;
        }
        bool first = true;
        for (const auto& [section, records] : sections) {
            io::ByteBuffer one;
            const std::int64_t packed = ((static_cast<std::int64_t>(chunkX) & 0x3FFFFF) << 42) |
                                        ((static_cast<std::int64_t>(chunkZ) & 0x3FFFFF) << 20) |
                                        (static_cast<std::int64_t>(section) & 0xFFFFF);
            one.writeLong(packed);
            pv::writeBoolean(one, true);
            pv::varInt::write(one, static_cast<std::int32_t>(records.size()));
            for (const std::int64_t record : records) {
                pv::varLong::write(one, record);
            }
            if (first) {
                out.writeBytes(one.readable());
                first = false;
            } else {
                packet.forwarded.emplace_back(kMultiBlockChangeNew, one.toVector());
            }
        }
        break;
    }
    case kUnlockRecipes: {
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeBytes(in.readSpan(4));
        out.writeBytes(std::vector<std::uint8_t>(4, 0));
        out.writeBytes(in.readable());
        break;
    }
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
