#include <basalt/wire/multiversion/CompactBitStorage.hpp>
#include <basalt/wire/multiversion/java/Link47.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::int16_t kEmptySlot = -1;
/// parity: ClientboundMoveEntityPacket, numbered as this server does.
constexpr std::int32_t kMoveEntityPos = 54;
constexpr std::int32_t kMoveEntityPosRot = 55;
constexpr double kMoveUnits = 4096.0;
constexpr std::int64_t kUnitsPerFixed = 128;
constexpr std::int32_t kTeleportEntity47 = 0x18;
constexpr std::uint8_t kEffectVisible = 2;

[[nodiscard]] constexpr std::int64_t floorDiv(std::int64_t value, std::int64_t divisor) {
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

constexpr std::uint8_t kUseInAir = 255;
constexpr std::size_t kCursorBytes = 3;
using protocol::ProtocolState;

constexpr std::int32_t kSections = 16;
constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr int kMaxIndirectBits = 8;
constexpr std::size_t kLightBytes = 2048;
constexpr std::size_t kColumns = 16 * 16;
constexpr std::size_t kVelocityBytes = 6;
constexpr std::size_t kPositionHeadBytes = 8 * 3 + 4 * 2 + 1;

constexpr std::int32_t kAddEntity = 0x00;
constexpr std::int32_t kAddExperienceOrb = 0x01;
constexpr std::int32_t kAddGlobalEntity = 0x02;
constexpr std::int32_t kAddMob = 0x03;
constexpr std::int32_t kAddPlayer = 0x05;
constexpr std::int32_t kNamedSound47 = 0x19;
constexpr std::int32_t kMapChunk47 = 0x21;

constexpr std::int32_t kClientInformation = 14;

constexpr std::int32_t kMainHand = 0;
constexpr std::int32_t kOffHand = 1;
constexpr std::int32_t kMainHandRight = 1;
constexpr std::int32_t kOverworld = 0;

/// parity: `EntityDataSerializer` ended a list with 127 before the indexed format.
constexpr std::uint8_t kEndOfData = 0x7F;

/// parity: a position was sent in thirty-seconds of a block.
constexpr double kFixedPoint = 32.0;

[[nodiscard]] std::int32_t fixedPoint(double value) {
    constexpr double kLimit = 2147483647.0;
    if (std::isnan(value)) {
        return 0;
    }
    return static_cast<std::int32_t>(std::clamp(value * kFixedPoint, -kLimit, kLimit));
}

void writeFixed(io::ByteBuffer& output, double x, double y, double z) {
    output.writeInt(fixedPoint(x));
    output.writeInt(fixedPoint(y));
    output.writeInt(fixedPoint(z));
}

[[nodiscard]] std::vector<std::int32_t> readContainer(io::ByteBuffer& input) {
    const int bits = input.readByte();
    std::vector<std::int32_t> palette;
    const bool indirect = bits <= kMaxIndirectBits;
    if (indirect) {
        const std::int32_t entries = protocol::varInt::read(input);
        palette.reserve(std::min(static_cast<std::size_t>(entries < 0 ? 0 : entries), input.readableBytes()));
        for (std::int32_t i = 0; i < entries && !input.failed(); ++i) {
            palette.push_back(protocol::varInt::read(input));
        }
    }
    const std::int32_t count = protocol::varInt::read(input);
    std::vector<std::int64_t> words;
    words.reserve(std::min(static_cast<std::size_t>(count < 0 ? 0 : count), input.readableBytes()));
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
    case play::UpdateMobEffectPacket::kId:
        // 1.9 replaced whether to hide the particles with flags, showing them being the second bit.
        if (!packet.body.empty()) {
            packet.body.back() = (packet.body.back() & kEffectVisible) != 0 ? 0 : 1;
        }
        break;
    case kMoveEntityPos:
        moveEntity(packet, false);
        break;
    case kMoveEntityPosRot:
        moveEntity(packet, true);
        break;
    case play::MapChunkPacket::kId:
        return rewriteChunk(packet);
    case play::ForgetLevelChunkPacket::kId:
        emptyChunk(packet);
        break;
    case play::SetEntityDataPacket::kId:
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
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeBytes(packet.body);
            protocol::varInt::write(output, kMainHandRight);
        });
        return true;
    }
    if (packet.nativeId() == play::ServerboundUseItemOnPacket::kId) {
        return splitPlacement(packet);
    }
    if (packet.nativeId() == play::ServerboundPositionLookPacket::kId && teleport_.has_value()) {
        // 1.8 answers a teleport with a move; from 1.9 it confirms the id first, then moves.
        io::ByteBuffer accept;
        protocol::varInt::write(accept, *teleport_);
        teleport_.reset();
        std::vector<std::uint8_t> move = std::move(packet.body);
        packet.reset(play::AcceptTeleportationPacket::kId, play::AcceptTeleportationPacket::kId, accept.toVector());
        packet.extras.emplace_back(play::ServerboundPositionLookPacket::kId, std::move(move));
    }
    return true;
}

bool Link47::splitPlacement(MutablePacket& packet) {
    // 1.8 sends one packet for both, a face of 255 meaning the item was used in the air; 1.9 has a packet each.
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int64_t pos = input.readLong();
    const std::uint8_t face = input.readByte();
    if (input.readShort() != kEmptySlot) {
        static_cast<void>(input.readByte());
        static_cast<void>(input.readShort());
        static_cast<void>(readNamedNbt(input));
    }
    const std::span<const std::uint8_t> cursor = input.readSpan(kCursorBytes);
    if (input.failed()) {
        return false;
    }
    if (face == kUseInAir) {
        io::ByteBuffer use;
        protocol::varInt::write(use, kMainHand);
        packet.reset(play::ServerboundUseItemPacket::kId, play::ServerboundUseItemPacket::kId, use.toVector());
        return true;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeLong(pos);
        protocol::varInt::write(output, face);
        protocol::varInt::write(output, kMainHand);
        output.writeBytes(cursor);
    });
    return true;
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
    if (packet.id == kAddExperienceOrb || packet.id == kAddGlobalEntity) {
        const std::uint8_t kind = packet.id == kAddGlobalEntity ? input.readByte() : 0;
        const double x = input.readDouble();
        const double y = input.readDouble();
        const double z = input.readDouble();
        remember(entityId, x, y, z, 0, 0);
        const std::vector<std::uint8_t> rest = input.readBytes(input.readable().size());
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            if (packet.id == kAddGlobalEntity) {
                output.writeByte(kind);
            }
            writeFixed(output, x, y, z);
            output.writeBytes(rest);
        });
        return true;
    }
    const std::int64_t mostSignificant = input.readLong();
    const std::int64_t leastSignificant = input.readLong();
    if (packet.spawn == MutablePacket::Spawn::kPainting) {
        // 1.9 gave a painting its uuid; the rest is as 1.8 reads it.
        const std::span<const std::uint8_t> rest = input.readable();
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            output.writeBytes(rest);
        });
        return true;
    }
    switch (packet.id) {
    case kAddEntity: {
        const std::uint8_t type = input.readByte();
        const double x = input.readDouble();
        const double y = input.readDouble();
        const double z = input.readDouble();
        const std::uint8_t pitch = input.readByte();
        const std::uint8_t yaw = input.readByte();
        const std::int32_t data = input.readInt();
        remember(entityId, x, y, z, yaw, pitch);
        const std::span<const std::uint8_t> readable = input.readable();
        const std::vector<std::uint8_t> velocity{readable.begin(), readable.end()};
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            output.writeByte(type);
            writeFixed(output, x, y, z);
            output.writeByte(pitch);
            output.writeByte(yaw);
            output.writeInt(data);
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
        remember(entityId, x, y, z, yaw, pitch);
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
        remember(entityId, x, y, z, yaw, pitch);
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

void Link47::remember(std::int32_t entityId, double x, double y, double z, std::uint8_t yaw, std::uint8_t pitch) {
    positions_[entityId] = Tracked{{static_cast<std::int64_t>(std::llround(x * kMoveUnits)),
                                    static_cast<std::int64_t>(std::llround(y * kMoveUnits)),
                                    static_cast<std::int64_t>(std::llround(z * kMoveUnits))},
                                   yaw,
                                   pitch};
}

void Link47::moveEntity(MutablePacket& packet, bool rotates) {
    // 1.9 widened a relative move from a 32nd of a block in a byte to a 4096th in a short.
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    std::array<std::int64_t, 3> delta{};
    for (std::int64_t& axis : delta) {
        axis = input.readShort();
    }
    const std::uint8_t yaw = rotates ? input.readByte() : 0;
    const std::uint8_t pitch = rotates ? input.readByte() : 0;
    const std::uint8_t onGround = input.readByte();
    if (input.failed()) {
        return;
    }
    const auto found = positions_.find(entityId);
    std::array<std::int32_t, 3> step{};
    std::array<std::int32_t, 3> fixed{};
    bool far = false;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const std::int64_t before = found == positions_.end() ? 0 : found->second.position[axis];
        const std::int64_t after = before + delta[axis];
        fixed[axis] = static_cast<std::int32_t>(floorDiv(after, kUnitsPerFixed));
        step[axis] = static_cast<std::int32_t>(floorDiv(after, kUnitsPerFixed) - floorDiv(before, kUnitsPerFixed));
        far = far || step[axis] < INT8_MIN || step[axis] > INT8_MAX;
        if (found != positions_.end()) {
            found->second.position[axis] = after;
        }
    }
    if (found != positions_.end() && rotates) {
        found->second.yaw = yaw;
        found->second.pitch = pitch;
    }
    if (far && found != positions_.end()) {
        // parity: EntityTrackerEntry, which sends a teleport when a move does not fit a byte.
        packet.overrideId(kTeleportEntity47);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            for (const std::int32_t axis : fixed) {
                output.writeInt(axis);
            }
            output.writeByte(found->second.yaw);
            output.writeByte(found->second.pitch);
            output.writeByte(onGround);
        });
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, entityId);
        for (const std::int32_t axis : step) {
            output.writeByte(static_cast<std::uint8_t>(std::clamp(axis, INT8_MIN, INT8_MAX)));
        }
        if (rotates) {
            output.writeByte(yaw);
            output.writeByte(pitch);
        }
        output.writeByte(onGround);
    });
}

void Link47::fixedPointTeleport(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    const double x = input.readDouble();
    const double y = input.readDouble();
    const double z = input.readDouble();
    const std::span<const std::uint8_t> rotation = input.readable();
    if (rotation.size() >= 2) {
        remember(entityId, x, y, z, rotation[0], rotation[1]);
    }
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
    const std::int32_t teleportId = protocol::varInt::read(input);
    if (!input.failed()) {
        teleport_ = teleportId;
    }
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
