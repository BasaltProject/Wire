#include <basalt/wire/multiversion/CompactBitStorage.hpp>
#include <basalt/wire/multiversion/java/Link578Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/BitStorage.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kSpawnWeather = 0x02;
constexpr std::int32_t kChat = 0x0F;
constexpr std::int32_t kChunk = 0x22;
constexpr std::int32_t kLight = 0x25;
constexpr std::int32_t kLogin = 0x26;
constexpr std::int32_t kRespawn = 0x3B;
constexpr std::int32_t kEquipment = 0x47;
constexpr std::int32_t kAttributes = 0x59;

constexpr std::int32_t kUseEntity = 0x0E;
constexpr std::int32_t kAbilities = 0x19;

/// parity: `EntityType.LIGHTNING_BOLT` in 1.16.
constexpr std::int32_t kLightningBolt = 41;
constexpr std::int32_t kSections = 16;
constexpr std::int32_t kBiomesInChunk = 1024;
constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr int kBlockMaxIndirectBits = 8;
constexpr int kHeightmapBits = 9;
constexpr std::size_t kHeightmapEntries = 256;
constexpr std::size_t kModifierBytes = 16 + 8 + 1;
constexpr std::int32_t kMoreSlots = 0x80;

constexpr std::array<std::string_view, 3> kDimensions = {"minecraft:overworld", "minecraft:the_nether",
                                                         "minecraft:the_end"};

/// parity: the keys of `Attributes` before 1.16 gave them a namespace and snake case.
constexpr std::array<std::pair<std::string_view, std::string_view>, 13> kLegacyNames = {{
    {"minecraft:generic.max_health", "generic.maxHealth"},
    {"minecraft:generic.follow_range", "generic.followRange"},
    {"minecraft:generic.knockback_resistance", "generic.knockbackResistance"},
    {"minecraft:generic.movement_speed", "generic.movementSpeed"},
    {"minecraft:generic.flying_speed", "generic.flyingSpeed"},
    {"minecraft:generic.attack_damage", "generic.attackDamage"},
    {"minecraft:generic.attack_knockback", "generic.attackKnockback"},
    {"minecraft:generic.attack_speed", "generic.attackSpeed"},
    {"minecraft:generic.armor", "generic.armor"},
    {"minecraft:generic.armor_toughness", "generic.armorToughness"},
    {"minecraft:generic.luck", "generic.luck"},
    {"minecraft:horse.jump_strength", "horse.jumpStrength"},
    {"minecraft:zombie.spawn_reinforcements", "zombie.spawnReinforcements"},
}};

[[nodiscard]] std::string modernName(const std::string& name) {
    for (const auto& [modern, legacy] : kLegacyNames) {
        if (legacy == name) {
            return std::string{modern};
        }
    }
    return name;
}

[[nodiscard]] std::string_view dimensionName(std::int32_t number) {
    return number == -1 ? kDimensions[1] : (number == 1 ? kDimensions[2] : kDimensions[0]);
}

void writeEmptyCodec(io::ByteBuffer& out) {
    out.writeByte(0x0a);
    out.writeShort(0);
    out.writeByte(0);
}

void padContainer(io::ByteBuffer& in, io::ByteBuffer& out) {
    const int bits = in.readByte();
    out.writeByte(static_cast<std::uint8_t>(bits));
    if (bits <= kBlockMaxIndirectBits) {
        const std::int32_t entries = pv::varInt::read(in);
        pv::varInt::write(out, entries);
        for (std::int32_t index = 0; index < entries && !in.failed(); ++index) {
            pv::varInt::write(out, pv::varInt::read(in));
        }
    }
    const std::int32_t count = pv::varInt::read(in);
    std::vector<std::int64_t> words;
    words.reserve(std::min(static_cast<std::size_t>(count < 0 ? 0 : count), in.readableBytes()));
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        words.push_back(in.readLong());
    }
    if (bits < 1 || bits > 32 || words.size() != compactBitStorage::longsFor(kBlocksPerSection, bits)) {
        in.markFailed();
        return;
    }
    const std::vector<std::int32_t> values = compactBitStorage::unpack(words, bits, kBlocksPerSection);
    std::vector<std::uint64_t> padded(protocol::play::bitStorageLongCount(bits, kBlocksPerSection), 0);
    protocol::play::BitStorageView storage{bits, kBlocksPerSection, padded};
    for (std::size_t index = 0; index < kBlocksPerSection; ++index) {
        storage.set(index, values[index]);
    }
    pv::varInt::write(out, static_cast<std::int32_t>(padded.size()));
    for (const std::uint64_t word : padded) {
        out.writeLong(static_cast<std::int64_t>(word));
    }
}

[[nodiscard]] nbt::NbtTag paddedHeightmaps(const nbt::NbtTag& heightmaps) {
    const nbt::NbtCompound* compound = heightmaps.get<nbt::NbtCompound>();
    if (compound == nullptr) {
        return heightmaps;
    }
    nbt::NbtCompound rewritten;
    for (std::size_t index = 0; index < compound->size(); ++index) {
        const std::string& name = compound->names()[index];
        const auto* data = compound->values()[index].get<std::vector<std::int64_t>>();
        if (data == nullptr) {
            rewritten.set(name, compound->values()[index]);
            continue;
        }
        if (data->size() != compactBitStorage::longsFor(kHeightmapEntries, kHeightmapBits)) {
            continue;
        }
        const std::vector<std::int32_t> values = compactBitStorage::unpack(*data, kHeightmapBits, kHeightmapEntries);
        std::vector<std::uint64_t> padded(protocol::play::bitStorageLongCount(kHeightmapBits, kHeightmapEntries), 0);
        protocol::play::BitStorageView storage{kHeightmapBits, kHeightmapEntries, padded};
        for (std::size_t entry = 0; entry < kHeightmapEntries; ++entry) {
            storage.set(entry, values[entry]);
        }
        rewritten.putLongArray(name, std::vector<std::int64_t>{padded.begin(), padded.end()});
    }
    return nbt::NbtTag::ofCompound(std::move(rewritten));
}

} // namespace

bool Link578Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kLogin: {
        out.writeInt(in.readInt());
        out.writeByte(in.readByte());
        const std::string_view dimension = dimensionName(in.readInt());
        const std::int64_t seed = in.readLong();
        const std::uint8_t maxPlayers = in.readByte();
        const std::string levelType = pv::readString(in);
        const std::int32_t viewDistance = pv::varInt::read(in);
        const std::uint8_t reduced = in.readByte();
        const std::uint8_t respawnScreen = in.readByte();
        out.writeByte(0xFF);
        pv::varInt::write(out, static_cast<std::int32_t>(kDimensions.size()));
        for (const std::string_view name : kDimensions) {
            pv::writeString(out, name);
        }
        writeEmptyCodec(out);
        pv::writeString(out, dimension);
        pv::writeString(out, dimension);
        out.writeLong(seed);
        out.writeByte(maxPlayers);
        pv::varInt::write(out, viewDistance);
        out.writeByte(reduced);
        out.writeByte(respawnScreen);
        pv::writeBoolean(out, false);
        pv::writeBoolean(out, levelType == "flat");
        break;
    }
    case kRespawn: {
        const std::string_view dimension = dimensionName(in.readInt());
        const std::int64_t seed = in.readLong();
        const std::uint8_t gamemode = in.readByte();
        const std::string levelType = pv::readString(in);
        pv::writeString(out, dimension);
        pv::writeString(out, dimension);
        out.writeLong(seed);
        out.writeByte(gamemode);
        out.writeByte(0xFF);
        pv::writeBoolean(out, false);
        pv::writeBoolean(out, levelType == "flat");
        pv::writeBoolean(out, false);
        break;
    }
    case kChat:
        out.writeBytes(in.readable());
        out.writeBytes(std::vector<std::uint8_t>(16, 0));
        in = io::ByteBuffer{};
        break;
    case kChunk: {
        out.writeInt(in.readInt());
        out.writeInt(in.readInt());
        const std::uint8_t full = in.readByte();
        out.writeByte(full);
        pv::writeBoolean(out, true);
        const std::int32_t mask = pv::varInt::read(in);
        pv::varInt::write(out, mask);
        const auto heightmaps = readNamedNbt(in);
        if (in.failed()) {
            return false;
        }
        writeNamedNbt(out, paddedHeightmaps(heightmaps->tag));
        if (full != 0) {
            out.writeBytes(in.readSpan(static_cast<std::size_t>(kBiomesInChunk) * 4));
        }
        const std::int32_t length = pv::varInt::read(in);
        io::ByteBuffer sections = io::ByteBuffer::wrap(in.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length)));
        io::ByteBuffer packed;
        for (std::int32_t index = 0; index < kSections && !in.failed(); ++index) {
            if ((mask & (1 << index)) == 0) {
                continue;
            }
            packed.writeShort(sections.readShort());
            padContainer(sections, packed);
        }
        if (sections.failed()) {
            return false;
        }
        pv::varInt::write(out, static_cast<std::int32_t>(packed.readableBytes()));
        out.writeBytes(packed.readable());
        out.writeBytes(in.readable());
        in = io::ByteBuffer{};
        break;
    }
    case kLight:
        pv::varInt::write(out, pv::varInt::read(in));
        pv::varInt::write(out, pv::varInt::read(in));
        pv::writeBoolean(out, true);
        out.writeBytes(in.readable());
        in = io::ByteBuffer{};
        break;
    case kEquipment: {
        pv::varInt::write(out, pv::varInt::read(in));
        const std::int32_t slot = pv::varInt::read(in);
        out.writeByte(static_cast<std::uint8_t>(slot & ~kMoreSlots));
        out.writeBytes(in.readable());
        in = io::ByteBuffer{};
        break;
    }
    case kAttributes: {
        pv::varInt::write(out, pv::varInt::read(in));
        const std::int32_t count = in.readInt();
        out.writeInt(count);
        for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
            pv::writeString(out, modernName(pv::readString(in)));
            out.writeDouble(in.readDouble());
            const std::int32_t modifiers = pv::varInt::read(in);
            pv::varInt::write(out, modifiers);
            out.writeBytes(in.readSpan(static_cast<std::size_t>(std::max(modifiers, 0)) * kModifierBytes));
        }
        break;
    }
    case kSpawnWeather: {
        const std::int32_t entity = pv::varInt::read(in);
        static_cast<void>(in.readByte());
        const double x = in.readDouble();
        const double y = in.readDouble();
        const double z = in.readDouble();
        pv::varInt::write(out, entity);
        out.writeBytes(std::vector<std::uint8_t>(16, 0));
        pv::varInt::write(out, kLightningBolt);
        out.writeDouble(x);
        out.writeDouble(y);
        out.writeDouble(z);
        out.writeShort(0);
        out.writeInt(0);
        out.writeBytes(std::vector<std::uint8_t>(6, 0));
        packet.overrideId(0x00);
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

bool Link578Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    if (packet.id == kUseEntity) {
        if (!packet.body.empty()) {
            packet.body.pop_back();
        }
    } else if (packet.id == kAbilities) {
        io::ByteBuffer out;
        out.writeBytes(packet.body);
        out.writeFloat(0.05F);
        out.writeFloat(0.1F);
        packet.body = out.toVector();
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
