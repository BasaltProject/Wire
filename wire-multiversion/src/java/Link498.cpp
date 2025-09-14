#include <basalt/wire/multiversion/java/Link498.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::int32_t kBiomesInChunk = 1024;
constexpr std::int32_t kColumns = 16 * 16;
constexpr std::int32_t kColumnSide = 16;
constexpr std::int32_t kBiomeCell = 4;
constexpr std::int32_t kBiomeSide = 4;

/// What 1.15 calls the two spawns that carry the entity's data with them.
constexpr std::int32_t kAddMob = 0x03;
constexpr std::int32_t kAddPlayer = 0x05;

/// parity: `EntityDataSerializer` ends a list with an index of 255.
constexpr std::uint8_t kEndOfData = 0xFF;

/// The biome of a column, which 1.15 has one of per four blocks of height and 1.14 one of.
[[nodiscard]] std::int32_t biomeOf(std::span<const std::int32_t> biomes, std::int32_t column) {
    const std::int32_t x = (column % kColumnSide) / kBiomeCell;
    const std::int32_t z = (column / kColumnSide) / kBiomeCell;
    const auto index = static_cast<std::size_t>(z * kBiomeSide + x);
    return index < biomes.size() ? biomes[index] : 0;
}

} // namespace

bool Link498::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::MapChunkPacket::kId:
        foldBiomesIn(packet);
        break;
    case play::LoginPlayPacket::kId:
        dropSeedAndRespawnScreen(packet);
        break;
    case play::RespawnPacket::kId:
        dropSeed(packet);
        break;
    case play::LevelParticlesPacket::kId:
        shortenCoordinates(packet);
        break;
    case play::SpawnEntityPacket::kId:
        // A mob and a player are the same packet here as far as this server is concerned, so which
        // one it is has to be read off the number 1.15 gave it.
        if (packet.id == kAddMob || packet.id == kAddPlayer) {
            appendEmptyData(packet);
        }
        break;
    default:
        break;
    }
    return true;
}

void Link498::foldBiomesIn(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = input.readInt();
    const std::int32_t z = input.readInt();
    const std::uint8_t fullChunk = input.readByte();
    const std::int32_t mask = protocol::varInt::read(input);
    const nbt::NbtTag heightmaps = protocol::readAnonymousNbt(input);
    std::vector<std::int32_t> biomes;
    biomes.reserve(kBiomesInChunk);
    for (std::int32_t i = 0; i < kBiomesInChunk && !input.failed(); ++i) {
        biomes.push_back(input.readInt());
    }
    const std::int32_t length = protocol::varInt::read(input);
    const std::vector<std::uint8_t> sections = input.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length));
    if (input.failed()) {
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(x);
        output.writeInt(z);
        output.writeByte(fullChunk);
        protocol::varInt::write(output, mask);
        protocol::writeAnonymousNbt(output, heightmaps);
        protocol::varInt::write(output, static_cast<std::int32_t>(sections.size()) +
                                            kColumns * static_cast<std::int32_t>(sizeof(std::int32_t)));
        output.writeBytes(sections);
        for (std::int32_t index = 0; index < kColumns; ++index) {
            output.writeInt(biomeOf(biomes, index));
        }
        output.writeBytes(input.readable());
    });
}

void Link498::dropSeedAndRespawnScreen(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = input.readInt();
    const std::uint8_t gamemode = input.readByte();
    const std::int32_t dimension = input.readInt();
    static_cast<void>(input.readLong());
    const std::uint8_t maxPlayers = input.readByte();
    const std::string levelType = protocol::readString(input);
    const std::int32_t viewDistance = protocol::varInt::read(input);
    const std::uint8_t reducedDebugInfo = input.readByte();
    if (input.failed()) {
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(entityId);
        output.writeByte(gamemode);
        output.writeInt(dimension);
        output.writeByte(maxPlayers);
        protocol::writeString(output, levelType);
        protocol::varInt::write(output, viewDistance);
        output.writeByte(reducedDebugInfo);
    });
}

void Link498::dropSeed(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t dimension = input.readInt();
    static_cast<void>(input.readLong());
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(dimension);
        output.writeBytes(input.readable());
    });
}

void Link498::shortenCoordinates(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t particle = input.readInt();
    const std::uint8_t longDistance = input.readByte();
    const double x = input.readDouble();
    const double y = input.readDouble();
    const double z = input.readDouble();
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(particle);
        output.writeByte(longDistance);
        output.writeFloat(static_cast<float>(x));
        output.writeFloat(static_cast<float>(y));
        output.writeFloat(static_cast<float>(z));
        output.writeBytes(input.readable());
    });
}

void Link498::appendEmptyData(MutablePacket& packet) {
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeBytes(packet.body);
        output.writeByte(kEndOfData);
    });
}

} // namespace basalt::wire::multiversion::java
