#include <basalt/wire/multiversion/java/Link754.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/BitStorage.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::int32_t kRemoveEntities754 = 0x36;
using protocol::ProtocolState;

/// parity: `ServerboundClientInformationPacket`.
constexpr std::int32_t kClientInformationConfiguration = 0x00;
constexpr std::int32_t kClientInformationPlay = 14;

/// This server's world: twenty four sections from -64, of which 1.16 can hold sixteen.
constexpr std::int32_t kTotalSections = 24;
constexpr std::int32_t kFirstSection = 4;
constexpr std::int32_t kLastSection = 19;
constexpr std::int32_t kWorldFloor = 64;
constexpr std::int32_t kMaxY = 255;

constexpr std::int32_t kTotalLightSections = kTotalSections + 2;
constexpr std::int32_t kFirstLightSection = kFirstSection;
constexpr std::int32_t kLightSectionsIn1_16 = 18;

constexpr int kBlockMaxIndirectBits = 8;
constexpr std::int32_t kBiomesPerSection = 4 * 4 * 4;
constexpr std::int32_t kBiomesIn1_16 = 1024;

constexpr int kHeightmapBits = 9;
constexpr std::size_t kHeightmapEntries = 256;

/// parity: `ClientboundSetTitlesPacket.Type`.
constexpr std::int32_t kTitle = 0x4F;
constexpr std::int32_t kSetTitle = 0;
constexpr std::int32_t kSetSubtitle = 1;
constexpr std::int32_t kSetActionBar = 2;
constexpr std::int32_t kSetTimes = 3;
constexpr std::int32_t kHide = 4;
constexpr std::int32_t kReset = 5;
constexpr std::int32_t kMaxText = 262144;

constexpr std::size_t kBitsPerWord = 64;

[[nodiscard]] bool isSet(std::span<const std::int64_t> mask, std::int32_t index) {
    const auto word = static_cast<std::size_t>(index) / kBitsPerWord;
    return word < mask.size() && (static_cast<std::uint64_t>(mask[word]) &
                                  (std::uint64_t{1} << (static_cast<std::size_t>(index) % kBitsPerWord))) != 0;
}

[[nodiscard]] std::vector<std::int64_t> readMask(io::ByteBuffer& input) {
    const std::int32_t count = protocol::varInt::read(input);
    std::vector<std::int64_t> mask;
    mask.reserve(std::min(static_cast<std::size_t>(count < 0 ? 0 : count), input.readableBytes()));
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        mask.push_back(input.readLong());
    }
    return mask;
}

void copyContainer(io::ByteBuffer& input, io::ByteBuffer& output, int maxIndirectBits) {
    const int bits = input.readByte();
    output.writeByte(static_cast<std::uint8_t>(bits));
    if (bits == 0) {
        protocol::varInt::write(output, protocol::varInt::read(input));
        protocol::varInt::write(output, protocol::varInt::read(input));
        return;
    }
    if (bits <= maxIndirectBits) {
        const std::int32_t entries = protocol::varInt::read(input);
        protocol::varInt::write(output, entries);
        for (std::int32_t i = 0; i < entries && !input.failed(); ++i) {
            protocol::varInt::write(output, protocol::varInt::read(input));
        }
    }
    const std::int32_t longs = protocol::varInt::read(input);
    protocol::varInt::write(output, longs);
    for (std::int32_t i = 0; i < longs && !input.failed(); ++i) {
        output.writeLong(input.readLong());
    }
}

[[nodiscard]] nbt::NbtCompound loweredHeightmaps(const nbt::NbtTag& tag) {
    const nbt::NbtCompound* root = tag.get<nbt::NbtCompound>();
    if (root == nullptr) {
        return {};
    }
    nbt::NbtCompound lowered;
    for (std::size_t i = 0; i < root->size(); ++i) {
        const auto* data = root->values()[i].get<std::vector<std::int64_t>>();
        if (data == nullptr) {
            continue;
        }
        if (data->size() != play::bitStorageLongCount(kHeightmapBits, kHeightmapEntries)) {
            continue;
        }
        std::vector<std::uint64_t> words;
        words.reserve(data->size());
        for (const std::int64_t word : *data) {
            words.push_back(static_cast<std::uint64_t>(word));
        }
        const play::BitStorage storage{kHeightmapBits, kHeightmapEntries, std::move(words)};
        play::BitStorage rebuilt{kHeightmapBits, kHeightmapEntries};
        for (std::size_t entry = 0; entry < kHeightmapEntries; ++entry) {
            rebuilt.set(entry, std::clamp(storage.get(entry) - kWorldFloor, 0, kMaxY + 1));
        }
        std::vector<std::int64_t> out;
        out.reserve(rebuilt.words().size());
        for (const std::uint64_t word : rebuilt.words()) {
            out.push_back(static_cast<std::int64_t>(word));
        }
        lowered.putLongArray(root->names()[i], std::move(out));
    }
    return lowered;
}

struct CutLight {
    std::int32_t mask = 0;
    std::vector<std::vector<std::uint8_t>> arrays;
};

[[nodiscard]] CutLight cutLightArrays(std::span<const std::int64_t> mask,
                                      const std::vector<std::vector<std::uint8_t>>& arrays) {
    CutLight cut;
    std::size_t arrayIndex = 0;
    for (std::int32_t section = 0; section < kTotalLightSections; ++section) {
        if (!isSet(mask, section)) {
            continue;
        }
        if (arrayIndex >= arrays.size()) {
            continue;
        }
        const std::vector<std::uint8_t>& array = arrays[arrayIndex++];
        const std::int32_t moved = section - kFirstLightSection;
        if (moved >= 0 && moved < kLightSectionsIn1_16) {
            cut.mask |= 1 << moved;
            cut.arrays.push_back(array);
        }
    }
    return cut;
}

[[nodiscard]] std::int32_t cutMask(std::span<const std::int64_t> mask) {
    std::int32_t out = 0;
    for (std::int32_t section = 0; section < kTotalLightSections; ++section) {
        if (!isSet(mask, section)) {
            continue;
        }
        const std::int32_t moved = section - kFirstLightSection;
        if (moved >= 0 && moved < kLightSectionsIn1_16) {
            out |= 1 << moved;
        }
    }
    return out;
}

} // namespace

bool Link754::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::MapChunkPacket::kId:
        return cutChunk(packet);
    case play::UpdateLightPacket::kId:
        return cutLight(packet);
    case play::BlockEntityDataPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const play::BlockPos pos = play::BlockPos::unpack(input.readLong());
        return !input.failed() && pos.y >= 0 && pos.y <= kMaxY;
    }
    case play::RemoveEntitiesPacket::kId: {
        // 1.17.0 removed one entity per packet; 1.16.5 takes a list again.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = protocol::varInt::read(input);
        if (input.failed()) {
            return false;
        }
        packet.overrideId(kRemoveEntities754);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, 1);
            protocol::varInt::write(output, entityId);
        });
        return true;
    }
    case play::PlayerPositionPacket::kId:
        // 1.17 added whether to dismount after the teleport id, and an angle after the spawn position.
        if (!packet.body.empty()) {
            packet.body.pop_back();
        }
        break;
    case play::SpawnPositionPacket::kId:
        packet.body.resize(std::min(packet.body.size(), sizeof(std::int64_t)));
        break;
    case play::ClientboundBlockUpdatePacket::kId: {
        const auto update =
            decode(packet, [](io::ByteBuffer& in) { return play::ClientboundBlockUpdatePacket::read(in); });
        return update.has_value() && update->pos.y >= 0 && update->pos.y <= kMaxY;
    }
    case play::UpdateAttributesPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = protocol::varInt::read(input);
        const std::int32_t count = protocol::varInt::read(input);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            output.writeInt(count);
            output.writeBytes(input.readable());
        });
        break;
    }
    case play::kSetTitleTextId:
        return asTitle(packet, kSetTitle);
    case play::kSetSubtitleTextId:
        return asTitle(packet, kSetSubtitle);
    case play::kSetActionBarTextId:
        return asTitle(packet, kSetActionBar);
    case play::SetTitlesAnimationPacket::kId: {
        const auto times = decode(packet, [](io::ByteBuffer& in) { return play::SetTitlesAnimationPacket::read(in); });
        if (!times.has_value()) {
            return true;
        }
        packet.overrideId(kTitle);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, kSetTimes);
            output.writeInt(times->fadeIn);
            output.writeInt(times->stay);
            output.writeInt(times->fadeOut);
        });
        break;
    }
    case play::ClearTitlesPacket::kId: {
        const auto clear = decode(packet, [](io::ByteBuffer& in) { return play::ClearTitlesPacket::read(in); });
        if (!clear.has_value()) {
            return true;
        }
        packet.overrideId(kTitle);
        encode(packet, [&](io::ByteBuffer& out) { protocol::varInt::write(out, clear->resetTimes ? kReset : kHide); });
        break;
    }
    case play::SectionBlocksUpdatePacket::kId: {
        // The packet names the section by its own coordinate, not by its index in the column, so only the range moves.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int64_t packed = input.readLong();
        const auto sectionY = static_cast<std::int32_t>((packed << 44) >> 44);
        if (input.failed() || sectionY < 0 || sectionY > kLastSection - kFirstSection) {
            return false;
        }
        break;
    }
    default:
        break;
    }
    return true;
}

bool Link754::asTitle(MutablePacket& packet, std::int32_t action) {
    const std::string json = decode(packet, [](io::ByteBuffer& in) { return protocol::readString(in, kMaxText); });
    packet.overrideId(kTitle);
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, action);
        protocol::writeString(output, json);
    });
    return true;
}

bool Link754::cutChunk(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = input.readInt();
    const std::int32_t z = input.readInt();
    const std::vector<std::int64_t> mask = readMask(input);
    const nbt::NbtTag heightmaps = readNamedTag(input);
    const std::int32_t biomeCount = protocol::varInt::read(input);
    std::vector<std::int32_t> biomes;
    biomes.reserve(std::min(static_cast<std::size_t>(biomeCount < 0 ? 0 : biomeCount), input.readableBytes()));
    for (std::int32_t i = 0; i < biomeCount && !input.failed(); ++i) {
        biomes.push_back(protocol::varInt::read(input));
    }
    const std::int32_t sectionBytes = protocol::varInt::read(input);
    io::ByteBuffer sections =
        io::ByteBuffer::wrap(input.readBytes(static_cast<std::size_t>(sectionBytes < 0 ? 0 : sectionBytes)));

    // The sections arrive in order, but only the ones the mask names are in the stream.
    std::int32_t newMask = 0;
    io::ByteBuffer kept;
    std::int32_t index = 0;
    while (sections.readableBytes() > 0 && !sections.failed()) {
        while (index < kTotalSections && !isSet(mask, index)) {
            index++;
        }
        if (index >= kTotalSections) {
            break;
        }
        io::ByteBuffer section;
        section.writeShort(sections.readShort());
        copyContainer(sections, section, kBlockMaxIndirectBits);
        if (index >= kFirstSection && index <= kLastSection) {
            newMask |= 1 << (index - kFirstSection);
            kept.writeBytes(section.readable());
        }
        index++;
    }

    std::vector<nbt::NbtTag> blockEntities;
    const std::int32_t entityCount = protocol::varInt::read(input);
    for (std::int32_t i = 0; i < entityCount && !input.failed(); ++i) {
        nbt::NbtTag tag = readNamedTag(input);
        const nbt::NbtCompound* entity = tag.get<nbt::NbtCompound>();
        if (entity != nullptr && entity->getInt("y") >= 0 && entity->getInt("y") <= kMaxY) {
            blockEntities.push_back(std::move(tag));
        }
    }

    const std::vector<std::uint8_t> data = kept.toVector();
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(x);
        output.writeInt(z);
        protocol::writeBoolean(output, true);
        protocol::varInt::write(output, newMask);
        writeNamedNbt(output, nbt::NbtTag::ofCompound(loweredHeightmaps(heightmaps)));
        protocol::varInt::write(output, kBiomesIn1_16);
        for (std::int32_t i = 0; i < kBiomesIn1_16; ++i) {
            const auto source = static_cast<std::size_t>(kFirstSection * kBiomesPerSection + i);
            protocol::varInt::write(output, source < biomes.size() ? biomes[source] : 0);
        }
        protocol::varInt::write(output, static_cast<std::int32_t>(data.size()));
        output.writeBytes(data);
        protocol::varInt::write(output, static_cast<std::int32_t>(blockEntities.size()));
        for (const nbt::NbtTag& tag : blockEntities) {
            writeNamedNbt(output, tag);
        }
    });
    return true;
}

bool Link754::cutLight(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = protocol::varInt::read(input);
    const std::int32_t z = protocol::varInt::read(input);
    const std::uint8_t trustEdges = input.readByte();
    const std::vector<std::int64_t> skyMask = readMask(input);
    const std::vector<std::int64_t> blockMask = readMask(input);
    const std::vector<std::int64_t> emptySkyMask = readMask(input);
    const std::vector<std::int64_t> emptyBlockMask = readMask(input);
    const auto readArrays = [&input] {
        const std::int32_t count = protocol::varInt::read(input);
        std::vector<std::vector<std::uint8_t>> arrays;
        arrays.reserve(std::min(static_cast<std::size_t>(count < 0 ? 0 : count), input.readableBytes()));
        for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
            const std::int32_t length = protocol::varInt::read(input);
            arrays.push_back(input.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length)));
        }
        return arrays;
    };
    const std::vector<std::vector<std::uint8_t>> skyLight = readArrays();
    const std::vector<std::vector<std::uint8_t>> blockLight = readArrays();
    if (input.failed()) {
        return true;
    }

    const CutLight sky = cutLightArrays(skyMask, skyLight);
    const CutLight block = cutLightArrays(blockMask, blockLight);
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, x);
        protocol::varInt::write(output, z);
        output.writeByte(trustEdges);
        protocol::varInt::write(output, sky.mask);
        protocol::varInt::write(output, block.mask);
        protocol::varInt::write(output, cutMask(emptySkyMask));
        protocol::varInt::write(output, cutMask(emptyBlockMask));
        for (const std::vector<std::uint8_t>& array : sky.arrays) {
            protocol::varInt::write(output, static_cast<std::int32_t>(array.size()));
            output.writeBytes(array);
        }
        for (const std::vector<std::uint8_t>& array : block.arrays) {
            protocol::varInt::write(output, static_cast<std::int32_t>(array.size()));
            output.writeBytes(array);
        }
    });
    return true;
}

bool Link754::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Play && packet.nativeId() == play::ServerboundContainerClickPacket::kId) {
        // The action number left with the confirmation 1.17 dropped; the stack the client predicted is not needed.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t container = input.readByte();
        const std::int16_t slot = input.readShort();
        const std::uint8_t button = input.readByte();
        static_cast<void>(input.readShort());
        const std::int32_t mode = protocol::varInt::read(input);
        if (input.failed()) {
            return false;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(container);
            output.writeShort(slot);
            output.writeByte(button);
            protocol::varInt::write(output, mode);
            protocol::varInt::write(output, 0);
            protocol::writeBoolean(output, false);
        });
        return true;
    }
    // What a client says about itself grew: 1.17 added whether it filters text.
    const bool settings =
        (state == ProtocolState::Configuration && packet.nativeId() == kClientInformationConfiguration) ||
        (state == ProtocolState::Play && packet.nativeId() == kClientInformationPlay);
    if (!settings) {
        return true;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeBytes(packet.body);
        output.writeByte(1);
    });
    return true;
}

} // namespace basalt::wire::multiversion::java
