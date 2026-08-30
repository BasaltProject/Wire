#include <basalt/wire/multiversion/java/Link772.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

/// parity: `ClientboundSetEntityMotionPacket` before 1.21.9, which stored blocks per tick times
/// 8000.
constexpr double kVelocityScale = 8000.0;
constexpr double kVelocityLimit = 3.9;

[[nodiscard]] std::int16_t quantize(double value) {
    return static_cast<std::int16_t>(std::lround(std::clamp(value, -kVelocityLimit, kVelocityLimit) * kVelocityScale));
}

/// The 256ths of a turn an angle travels as.
[[nodiscard]] std::uint8_t angleOf(float degrees) {
    return static_cast<std::uint8_t>(std::lround(degrees * 256.0F / 360.0F));
}

void writeShortVelocity(io::ByteBuffer& output, double x, double y, double z) {
    output.writeShort(quantize(x));
    output.writeShort(quantize(y));
    output.writeShort(quantize(z));
}

} // namespace

bool Link772::clientbound(ProtocolState state, MutablePacket& packet) {
    // Renumbering first, shape second: the numbers are read out of the packet as this server writes
    // it, and what is written here is no longer that packet.
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::SetEntityMotionPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            const auto motion = play::SetEntityMotionPacket::read(input);
            if (!motion.has_value()) {
                return;
            }
            protocol::varInt::write(output, motion->entityId);
            writeShortVelocity(output, motion->velocityX, motion->velocityY, motion->velocityZ);
        });
        break;
    case play::SpawnEntityPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            const auto spawn = play::SpawnEntityPacket::read(input);
            if (!spawn.has_value()) {
                return;
            }
            protocol::varInt::write(output, spawn->entityId);
            output.writeLong(static_cast<std::int64_t>(spawn->uuid.high()));
            output.writeLong(static_cast<std::int64_t>(spawn->uuid.low()));
            protocol::varInt::write(output, spawn->entityType);
            output.writeDouble(spawn->x);
            output.writeDouble(spawn->y);
            output.writeDouble(spawn->z);
            output.writeByte(angleOf(spawn->pitch));
            output.writeByte(angleOf(spawn->yaw));
            output.writeByte(angleOf(spawn->headYaw));
            protocol::varInt::write(output, spawn->objectData);
            writeShortVelocity(output, spawn->velocityX, spawn->velocityY, spawn->velocityZ);
        });
        break;
    case play::ExplodePacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            const auto explode = play::ExplodePacket::read(input);
            if (!explode.has_value()) {
                return;
            }
            output.writeDouble(explode->x);
            output.writeDouble(explode->y);
            output.writeDouble(explode->z);
            protocol::writeBoolean(output, explode->knockbackX.has_value());
            if (explode->knockbackX.has_value()) {
                output.writeDouble(*explode->knockbackX);
                output.writeDouble(explode->knockbackY);
                output.writeDouble(explode->knockbackZ);
            }
            protocol::varInt::write(output, explode->particleId);
            // The sound as a direct holder, which is how this server always writes one.
            protocol::varInt::write(output, 0);
            protocol::writeString(output, explode->sound);
            protocol::writeBoolean(output, false);
        });
        break;
    case play::SpawnPositionPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            const auto spawn = play::SpawnPositionPacket::read(input);
            if (!spawn.has_value()) {
                return;
            }
            // The dimension and the pitch have nowhere to go: 1.21.8 has one spawn point, in the
            // world the player is in, and one angle to face it with.
            output.writeLong(spawn->globalPos.location.pack());
            output.writeFloat(spawn->yaw);
        });
        break;
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
