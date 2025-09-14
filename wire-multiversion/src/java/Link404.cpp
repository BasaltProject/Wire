#include <basalt/wire/multiversion/java/Link404.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <array>
#include <bit>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::int32_t kAddEntity = 0x00;
constexpr std::int32_t kSections = 16;
constexpr int kMaxIndirectBits = 8;
constexpr std::int32_t kMapChunk = 0x22;
constexpr std::int32_t kOverworld = 0;

/// parity: the difficulty stopped travelling with the world, so it is told the usual one.
constexpr std::uint8_t kNormal = 2;

/// A section with no light of its own is dark rather than absent: 1.13 has no way to say.
constexpr std::array<std::uint8_t, 2048> kDark{};

void copyContainer(io::ByteBuffer& input, io::ByteBuffer& output) {
    const int bits = input.readByte();
    output.writeByte(static_cast<std::uint8_t>(bits));
    if (bits <= kMaxIndirectBits) {
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

[[nodiscard]] std::vector<std::vector<std::uint8_t>> readArrays(io::ByteBuffer& input, int count) {
    std::vector<std::vector<std::uint8_t>> arrays;
    arrays.reserve(static_cast<std::size_t>(count < 0 ? 0 : count));
    for (int i = 0; i < count && !input.failed(); ++i) {
        const std::int32_t length = protocol::varInt::read(input);
        arrays.push_back(input.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length)));
    }
    return arrays;
}

} // namespace

bool Link404::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::MapChunkPacket::kId:
        holdChunk(packet);
        return false;
    case play::UpdateLightPacket::kId:
        return lightTheChunk(packet);
    case play::LoginPlayPacket::kId:
        rewriteLogin(packet);
        break;
    case play::RespawnPacket::kId:
        rewriteRespawn(packet);
        break;
    case play::ChangeDifficultyPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t difficulty = input.readByte();
        encode(packet, [&](io::ByteBuffer& out) { out.writeByte(difficulty); });
        break;
    }
    case play::SpawnEntityPacket::kId:
        // Only the generic spawn: a mob or a player is its own packet by here, with its own shape,
        // and the links above made it one.
        if (packet.id == kAddEntity) {
            narrowType(packet);
        }
        break;
    default:
        break;
    }
    return true;
}

void Link404::holdChunk(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    PendingChunk held;
    held.x = input.readInt();
    held.z = input.readInt();
    held.fullChunk = input.readByte();
    held.mask = protocol::varInt::read(input);
    static_cast<void>(protocol::readAnonymousNbt(input));
    const std::int32_t length = protocol::varInt::read(input);
    held.sections = input.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length));
    const std::span<const std::uint8_t> rest = input.readable();
    held.rest.assign(rest.begin(), rest.end());
    if (input.failed()) {
        return;
    }
    pending_ = std::move(held);
}

bool Link404::lightTheChunk(MutablePacket& packet) {
    if (!pending_.has_value()) {
        return false;
    }
    const PendingChunk held = *std::move(pending_);
    pending_.reset();
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = protocol::varInt::read(input);
    const std::int32_t z = protocol::varInt::read(input);
    if (x != held.x || z != held.z) {
        return false;
    }
    const std::int32_t skyMask = protocol::varInt::read(input);
    const std::int32_t blockMask = protocol::varInt::read(input);
    static_cast<void>(protocol::varInt::read(input));
    static_cast<void>(protocol::varInt::read(input));
    // How many arrays there are is the masks: the count is not sent.
    const std::vector<std::vector<std::uint8_t>> skyLight =
        readArrays(input, std::popcount(static_cast<std::uint32_t>(skyMask)));
    const std::vector<std::vector<std::uint8_t>> blockLight =
        readArrays(input, std::popcount(static_cast<std::uint32_t>(blockMask)));

    io::ByteBuffer sections = io::ByteBuffer::wrap(held.sections);
    io::ByteBuffer data;
    std::size_t sky = 0;
    std::size_t block = 0;
    for (std::int32_t index = 0; index < kSections; ++index) {
        const std::vector<std::uint8_t>* skyHere = nullptr;
        const std::vector<std::uint8_t>* blockHere = nullptr;
        if ((skyMask & (1 << (index + 1))) != 0 && sky < skyLight.size()) {
            skyHere = &skyLight[sky++];
        }
        if ((blockMask & (1 << (index + 1))) != 0 && block < blockLight.size()) {
            blockHere = &blockLight[block++];
        }
        if ((held.mask & (1 << index)) == 0) {
            continue;
        }
        static_cast<void>(sections.readShort());
        copyContainer(sections, data);
        data.writeBytes(blockHere == nullptr ? std::span<const std::uint8_t>{kDark} : std::span{*blockHere});
        if (hasSkyLight_) {
            data.writeBytes(skyHere == nullptr ? std::span<const std::uint8_t>{kDark} : std::span{*skyHere});
        }
    }
    data.writeBytes(sections.readable());

    const std::vector<std::uint8_t> body = data.toVector();
    // From here down it is a chunk, not the light it arrived as: the links below have to know.
    packet.becomes(play::MapChunkPacket::kId);
    packet.overrideId(kMapChunk);
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(held.x);
        output.writeInt(held.z);
        output.writeByte(held.fullChunk);
        protocol::varInt::write(output, held.mask);
        protocol::varInt::write(output, static_cast<std::int32_t>(body.size()));
        output.writeBytes(body);
        output.writeBytes(held.rest);
    });
    return true;
}

void Link404::rewriteLogin(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = input.readInt();
    const std::uint8_t gamemode = input.readByte();
    const std::int32_t dimension = input.readInt();
    const std::uint8_t maxPlayers = input.readByte();
    const std::string levelType = protocol::readString(input);
    static_cast<void>(protocol::varInt::read(input));
    const std::uint8_t reducedDebugInfo = input.readByte();
    if (input.failed()) {
        return;
    }
    hasSkyLight_ = dimension == kOverworld;
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(entityId);
        output.writeByte(gamemode);
        output.writeInt(dimension);
        output.writeByte(kNormal);
        output.writeByte(maxPlayers);
        protocol::writeString(output, levelType);
        output.writeByte(reducedDebugInfo);
    });
}

void Link404::rewriteRespawn(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t dimension = input.readInt();
    const std::uint8_t gamemode = input.readByte();
    const std::string levelType = protocol::readString(input);
    if (input.failed()) {
        return;
    }
    hasSkyLight_ = dimension == kOverworld;
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(dimension);
        output.writeByte(kNormal);
        output.writeByte(gamemode);
        protocol::writeString(output, levelType);
    });
}

void Link404::narrowType(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    const std::int64_t mostSignificant = input.readLong();
    const std::int64_t leastSignificant = input.readLong();
    const std::int32_t type = protocol::varInt::read(input);
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, entityId);
        output.writeLong(mostSignificant);
        output.writeLong(leastSignificant);
        output.writeByte(static_cast<std::uint8_t>(type));
        output.writeBytes(input.readable());
    });
}

} // namespace basalt::wire::multiversion::java
