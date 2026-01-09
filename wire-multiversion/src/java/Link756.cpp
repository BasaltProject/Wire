#include <basalt/wire/multiversion/java/Link756.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/BitStorage.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cstdint>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

/// parity: `ServerboundClientInformationPacket`, which sits in both phases.
constexpr std::int32_t kClientInformationConfiguration = 0x00;
constexpr std::int32_t kClientInformationPlay = 14;

constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr std::size_t kBiomesPerSection = 4 * 4 * 4;
constexpr int kBlockMaxIndirectBits = 8;
constexpr int kBiomeMaxIndirectBits = 3;

/// parity: `PalettedContainer` never goes below four bits for blocks.
constexpr int kSmallestPalette = 4;

constexpr std::size_t kBitsPerWord = 64;

void setBit(std::vector<std::int64_t>& mask, std::size_t index) {
    const std::size_t word = index / kBitsPerWord;
    while (mask.size() <= word) {
        mask.push_back(0);
    }
    mask[word] |= static_cast<std::int64_t>(std::uint64_t{1} << (index % kBitsPerWord));
}

/// Copies one container across without decoding it, length prefix and all.
///
/// The exception is a container of a single value, which is 1.18's and which a client below it reads
/// as a four-bit palette of one entry: written as one, since that is the same section.
void copyContainer(io::ByteBuffer& input, io::ByteBuffer& output, std::size_t size, int maxIndirectBits) {
    const int bits = input.readByte();
    if (bits == 0) {
        const std::int32_t value = protocol::varInt::read(input);
        static_cast<void>(protocol::varInt::read(input));
        output.writeByte(static_cast<std::uint8_t>(kSmallestPalette));
        protocol::varInt::write(output, 1);
        protocol::varInt::write(output, value);
        const auto longs = static_cast<std::int32_t>(size * static_cast<std::size_t>(kSmallestPalette) / kBitsPerWord);
        protocol::varInt::write(output, longs);
        for (std::int32_t i = 0; i < longs; ++i) {
            output.writeLong(0);
        }
        return;
    }
    output.writeByte(static_cast<std::uint8_t>(bits));
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

/// The values of one container, which is what the biomes have to become.
[[nodiscard]] std::vector<std::int32_t> readContainer(io::ByteBuffer& input, std::size_t size, int maxIndirectBits) {
    const int bits = input.readByte();
    if (bits == 0) {
        const std::int32_t value = protocol::varInt::read(input);
        static_cast<void>(protocol::varInt::read(input)); // The length, zero for a container of one value.
        return std::vector<std::int32_t>(size, value);
    }
    std::vector<std::int32_t> palette;
    const bool indirect = bits <= maxIndirectBits;
    if (indirect) {
        const std::int32_t entries = protocol::varInt::read(input);
        palette.reserve(static_cast<std::size_t>(entries < 0 ? 0 : entries));
        for (std::int32_t i = 0; i < entries && !input.failed(); ++i) {
            palette.push_back(protocol::varInt::read(input));
        }
    }
    const std::int32_t longs = protocol::varInt::read(input);
    std::vector<std::uint64_t> words;
    words.reserve(static_cast<std::size_t>(longs < 0 ? 0 : longs));
    for (std::int32_t i = 0; i < longs && !input.failed(); ++i) {
        words.push_back(static_cast<std::uint64_t>(input.readLong()));
    }
    const play::BitStorageView storage{bits, size, words};
    std::vector<std::int32_t> values;
    values.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        const std::int32_t raw = storage.get(i);
        if (!indirect) {
            values.push_back(raw);
        } else if (raw >= 0 && static_cast<std::size_t>(raw) < palette.size()) {
            values.push_back(palette[static_cast<std::size_t>(raw)]);
        } else {
            values.push_back(0);
        }
    }
    return values;
}

} // namespace

bool Link756::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    if (packet.nativeId() == play::LoginPlayPacket::kId) {
        dropSimulationDistance(packet);
        return true;
    }
    if (packet.nativeId() != play::MapChunkPacket::kId) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = input.readInt();
    const std::int32_t z = input.readInt();
    const nbt::NbtTag heightmaps = protocol::readAnonymousNbt(input);
    const std::int32_t sectionBytes = protocol::varInt::read(input);
    io::ByteBuffer sections =
        io::ByteBuffer::wrap(input.readBytes(static_cast<std::size_t>(sectionBytes < 0 ? 0 : sectionBytes)));

    std::vector<std::int64_t> mask;
    std::vector<std::int32_t> biomes;
    io::ByteBuffer kept;
    std::size_t index = 0;
    while (sections.readableBytes() > 0 && !sections.failed()) {
        const std::int16_t nonAir = sections.readShort();
        io::ByteBuffer blocks;
        copyContainer(sections, blocks, kBlocksPerSection, kBlockMaxIndirectBits);
        const std::vector<std::int32_t> sectionBiomes =
            readContainer(sections, kBiomesPerSection, kBiomeMaxIndirectBits);
        biomes.insert(biomes.end(), sectionBiomes.begin(), sectionBiomes.end());
        if (nonAir != 0) {
            setBit(mask, index);
            kept.writeShort(nonAir);
            kept.writeBytes(blocks.readable());
        }
        index++;
    }

    const std::vector<std::uint8_t> data = kept.toVector();
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(x);
        output.writeInt(z);
        protocol::varInt::write(output, static_cast<std::int32_t>(mask.size()));
        for (const std::int64_t word : mask) {
            output.writeLong(word);
        }
        protocol::writeAnonymousNbt(output, heightmaps);
        protocol::varInt::write(output, static_cast<std::int32_t>(biomes.size()));
        for (const std::int32_t biome : biomes) {
            protocol::varInt::write(output, biome);
        }
        protocol::varInt::write(output, static_cast<std::int32_t>(data.size()));
        output.writeBytes(data);
        protocol::varInt::write(output, 0);
    });
    return true;
}

void Link756::dropSimulationDistance(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = input.readInt();
    const std::uint8_t hardcore = input.readByte();
    const std::uint8_t gamemode = input.readByte();
    const std::uint8_t previousGamemode = input.readByte();
    const std::int32_t worlds = protocol::varInt::read(input);
    std::vector<std::string> worldNames;
    for (std::int32_t i = 0; i < worlds && !input.failed(); ++i) {
        worldNames.push_back(protocol::readString(input));
    }
    const nbt::NbtTag codec = protocol::readAnonymousNbt(input);
    const nbt::NbtTag dimension = protocol::readAnonymousNbt(input);
    const std::string worldName = protocol::readString(input);
    const std::int64_t hashedSeed = input.readLong();
    const std::int32_t maxPlayers = protocol::varInt::read(input);
    const std::int32_t viewDistance = protocol::varInt::read(input);
    static_cast<void>(protocol::varInt::read(input));
    if (input.failed()) {
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(entityId);
        output.writeByte(hardcore);
        output.writeByte(gamemode);
        output.writeByte(previousGamemode);
        protocol::varInt::write(output, static_cast<std::int32_t>(worldNames.size()));
        for (const std::string& name : worldNames) {
            protocol::writeString(output, name);
        }
        protocol::writeAnonymousNbt(output, codec);
        protocol::writeAnonymousNbt(output, dimension);
        protocol::writeString(output, worldName);
        output.writeLong(hashedSeed);
        protocol::varInt::write(output, maxPlayers);
        protocol::varInt::write(output, viewDistance);
        output.writeBytes(input.readable());
    });
}

bool Link756::serverbound(ProtocolState state, MutablePacket& packet) {
    // What a client says about itself grew: 1.18 added whether it wants to be listed.
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
