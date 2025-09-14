#include <basalt/wire/multiversion/CompactBitStorage.hpp>
#include <basalt/wire/multiversion/java/Link47.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

constexpr std::int32_t kSections = 16;
constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr int kMaxIndirectBits = 8;
constexpr std::size_t kLightBytes = 2048;
constexpr std::size_t kColumns = 16 * 16;
constexpr std::size_t kVelocityBytes = 6;
constexpr std::size_t kPositionHeadBytes = 8 * 3 + 4 * 2 + 1;

/// What 1.9 calls the packets this one has to know apart, in that version's numbering.
constexpr std::int32_t kAddEntity = 0x00;
constexpr std::int32_t kAddMob = 0x03;
constexpr std::int32_t kAddPlayer = 0x05;
constexpr std::int32_t kNamedSound47 = 0x19;
constexpr std::int32_t kMapChunk47 = 0x21;

/// parity: `ServerboundClientInformationPacket` in play, which is where 1.8 sends it.
constexpr std::int32_t kClientInformation = 14;

constexpr std::int32_t kMainHand = 0;
constexpr std::int32_t kOffHand = 1;
constexpr std::int32_t kMainHandRight = 1;
constexpr std::int32_t kOverworld = 0;

/// parity: `EntityDataSerializer` ended a list with 127 before the indexed format.
constexpr std::uint8_t kEndOfData = 0x7F;

/// parity: a position was sent in thirty-seconds of a block.
constexpr double kFixedPoint = 32.0;

void writeFixed(io::ByteBuffer& output, double x, double y, double z) {
    output.writeInt(static_cast<std::int32_t>(x * kFixedPoint));
    output.writeInt(static_cast<std::int32_t>(y * kFixedPoint));
    output.writeInt(static_cast<std::int32_t>(z * kFixedPoint));
}

/// The values of one section, whatever width and palette it was written with.
[[nodiscard]] std::vector<std::int32_t> readContainer(io::ByteBuffer& input) {
    const int bits = input.readByte();
    std::vector<std::int32_t> palette;
    const bool indirect = bits <= kMaxIndirectBits;
    if (indirect) {
        const std::int32_t entries = protocol::varInt::read(input);
        palette.reserve(static_cast<std::size_t>(entries < 0 ? 0 : entries));
        for (std::int32_t i = 0; i < entries && !input.failed(); ++i) {
            palette.push_back(protocol::varInt::read(input));
        }
    }
    const std::int32_t count = protocol::varInt::read(input);
    std::vector<std::int64_t> words;
    words.reserve(static_cast<std::size_t>(count < 0 ? 0 : count));
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        words.push_back(input.readLong());
    }
    std::vector<std::int32_t> values = compactBitStorage::unpack(words, bits, kBlocksPerSection);
    if (!indirect) {
        return values;
    }
    for (std::int32_t& value : values) {
        value = value >= 0 && static_cast<std::size_t>(value) < palette.size()
                    ? palette[static_cast<std::size_t>(value)]
                    : 0;
    }
    return values;
}

} // namespace

bool Link47::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::MapChunkPacket::kId:
        return rewriteChunk(packet);
    case play::ForgetLevelChunkPacket::kId:
        emptyChunk(packet);
        break;
    case play::SetEntityDataPacket::kId:
        // The data arrives in 1.8's own shape, kind and index in one byte, so it goes through.
        break;
    case play::SpawnEntityPacket::kId:
        return rewriteSpawn(packet);
    case play::EntityTeleportPacket::kId:
    case play::EntityPositionSyncPacket::kId:
        fixedPointTeleport(packet);
        break;
    case play::SetEquipmentPacket::kId:
        return rewriteEquipment(packet);
    case play::SoundPacket::kId:
        dropCategory(packet);
        break;
    case play::PlayerPositionPacket::kId:
        dropTeleportId(packet);
        break;
    case play::LoginPlayPacket::kId:
        rememberDimension(packet, true);
        break;
    case play::RespawnPacket::kId:
        rememberDimension(packet, false);
        break;
    default:
        break;
    }
    return true;
}

bool Link47::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    if (packet.nativeId() == kClientInformation) {
        // 1.9 asked which hand is the main one. There is one hand there, and it is the right.
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeBytes(packet.body);
            protocol::varInt::write(output, kMainHandRight);
        });
        return true;
    }
    // Placing a block is the one thing a 1.8 client says that this server cannot read: it sends the
    // stack it believes it is holding and the cursor in sixteenths, and what reads it here wants a
    // hand, a hit and a sequence number that 1.8 has no idea about. Dropped rather than half-read: a
    // misread packet takes the connection with it.
    return packet.nativeId() != play::ServerboundUseItemOnPacket::kId;
}

bool Link47::rewriteChunk(MutablePacket& packet) const {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = input.readInt();
    const std::int32_t z = input.readInt();
    const bool fullChunk = input.readByte() != 0;
    const std::int32_t mask = protocol::varInt::read(input);
    const std::int32_t length = protocol::varInt::read(input);
    io::ByteBuffer data = io::ByteBuffer::wrap(input.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length)));

    io::ByteBuffer blocks;
    io::ByteBuffer blockLight;
    io::ByteBuffer skyLight;
    for (std::int32_t index = 0; index < kSections; ++index) {
        if ((mask & (1 << index)) == 0) {
            continue;
        }
        for (const std::int32_t value : readContainer(data)) {
            blocks.writeByte(static_cast<std::uint8_t>(value & 0xFF));
            blocks.writeByte(static_cast<std::uint8_t>((static_cast<std::uint32_t>(value) >> 8) & 0xFF));
        }
        blockLight.writeBytes(data.readBytes(kLightBytes));
        if (hasSkyLight_) {
            skyLight.writeBytes(data.readBytes(kLightBytes));
        }
    }
    const std::vector<std::uint8_t> biomes =
        fullChunk ? data.readBytes(std::min(kColumns, data.readableBytes())) : std::vector<std::uint8_t>{};

    io::ByteBuffer body;
    body.writeBytes(blocks.readable());
    body.writeBytes(blockLight.readable());
    body.writeBytes(skyLight.readable());
    body.writeBytes(biomes);
    const std::vector<std::uint8_t> whole = body.toVector();
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(x);
        output.writeInt(z);
        protocol::writeBoolean(output, fullChunk);
        output.writeShort(static_cast<std::int16_t>(mask));
        protocol::varInt::write(output, static_cast<std::int32_t>(whole.size()));
        output.writeBytes(whole);
    });
    return true;
}

void Link47::emptyChunk(MutablePacket& packet) {
    const auto chunk = decode(packet, [](io::ByteBuffer& in) { return play::ForgetLevelChunkPacket::read(in); });
    if (!chunk.has_value()) {
        return;
    }
    packet.overrideId(kMapChunk47);
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(chunk->chunkX);
        output.writeInt(chunk->chunkZ);
        protocol::writeBoolean(output, true);
        output.writeShort(0);
        protocol::varInt::write(output, 0);
    });
}

bool Link47::rewriteSpawn(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    const std::int64_t mostSignificant = input.readLong();
    const std::int64_t leastSignificant = input.readLong();
    switch (packet.id) {
    case kAddEntity: {
        const std::uint8_t type = input.readByte();
        const double x = input.readDouble();
        const double y = input.readDouble();
        const double z = input.readDouble();
        const std::uint8_t pitch = input.readByte();
        const std::uint8_t yaw = input.readByte();
        const std::int32_t data = input.readInt();
        const std::span<const std::uint8_t> readable = input.readable();
        const std::vector<std::uint8_t> velocity{readable.begin(), readable.end()};
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            output.writeByte(type);
            writeFixed(output, x, y, z);
            output.writeByte(pitch);
            output.writeByte(yaw);
            output.writeInt(data);
            // 1.8 only carries the velocity when there is object data to go with it.
            if (data != 0) {
                output.writeBytes(velocity);
            }
        });
        break;
    }
    case kAddMob: {
        const std::uint8_t type = input.readByte();
        const double x = input.readDouble();
        const double y = input.readDouble();
        const double z = input.readDouble();
        const std::uint8_t yaw = input.readByte();
        const std::uint8_t pitch = input.readByte();
        const std::uint8_t headPitch = input.readByte();
        const std::vector<std::uint8_t> velocity = input.readBytes(kVelocityBytes);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            output.writeByte(type);
            writeFixed(output, x, y, z);
            output.writeByte(yaw);
            output.writeByte(pitch);
            output.writeByte(headPitch);
            output.writeBytes(velocity);
            output.writeByte(kEndOfData);
        });
        break;
    }
    case kAddPlayer: {
        const double x = input.readDouble();
        const double y = input.readDouble();
        const double z = input.readDouble();
        const std::uint8_t yaw = input.readByte();
        const std::uint8_t pitch = input.readByte();
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            output.writeLong(mostSignificant);
            output.writeLong(leastSignificant);
            writeFixed(output, x, y, z);
            output.writeByte(yaw);
            output.writeByte(pitch);
            output.writeShort(0);
            output.writeByte(kEndOfData);
        });
        break;
    }
    default:
        return false;
    }
    return true;
}

void Link47::fixedPointTeleport(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    const double x = input.readDouble();
    const double y = input.readDouble();
    const double z = input.readDouble();
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, entityId);
        writeFixed(output, x, y, z);
        output.writeBytes(input.readable());
    });
}

bool Link47::rewriteEquipment(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    const std::int32_t slot = protocol::varInt::read(input);
    if (slot == kOffHand) {
        return false;
    }
    const std::int32_t old = slot == kMainHand ? 0 : slot - 1;
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, entityId);
        output.writeShort(static_cast<std::int16_t>(old));
        output.writeBytes(input.readable());
    });
    return true;
}

void Link47::dropCategory(MutablePacket& packet) {
    if (packet.id != kNamedSound47) {
        return;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::string name = protocol::readString(input);
    static_cast<void>(protocol::varInt::read(input));
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::writeString(output, name);
        output.writeBytes(input.readable());
    });
}

void Link47::dropTeleportId(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::vector<std::uint8_t> head = input.readBytes(kPositionHeadBytes);
    encode(packet, [&](io::ByteBuffer& out) { out.writeBytes(head); });
}

void Link47::rememberDimension(MutablePacket& packet, bool afterEntityAndGamemode) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    if (afterEntityAndGamemode) {
        static_cast<void>(input.readInt());
        static_cast<void>(input.readByte());
    }
    hasSkyLight_ = static_cast<std::int8_t>(input.readByte()) == kOverworld;
}

} // namespace basalt::wire::multiversion::java
