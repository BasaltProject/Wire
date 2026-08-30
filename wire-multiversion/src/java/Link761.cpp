#include <basalt/wire/multiversion/java/Link761.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::int32_t kAddPlayer761 = 2;
constexpr std::int32_t kEntityEvent761 = 25;
constexpr std::int32_t kAnimate761 = 3;

/// parity: `EntityEvent.HURT` and `AnimateAction.HURT`, the two halves of being hit.
constexpr std::uint8_t kHurtEvent = 2;
constexpr std::uint8_t kHurtAnimation = 1;

constexpr std::int32_t kPracticallyForever = 20 * 60 * 60 * 24;

} // namespace

bool Link761::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::SpawnEntityPacket::kId: {
        // The shape here is the one the link to 1.21.8 left: the velocity is three shorts at the end
        // and the angles are bytes.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = protocol::varInt::read(input);
        const std::int64_t mostSignificant = input.readLong();
        const std::int64_t leastSignificant = input.readLong();
        const std::int32_t type = protocol::varInt::read(input);
        if (!target_->isPlayer(type)) {
            return true;
        }
        const double x = input.readDouble();
        const double y = input.readDouble();
        const double z = input.readDouble();
        const std::uint8_t pitch = input.readByte();
        const std::uint8_t yaw = input.readByte();
        if (input.failed()) {
            return true;
        }
        packet.overrideId(kAddPlayer761);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            output.writeLong(mostSignificant);
            output.writeLong(leastSignificant);
            output.writeDouble(x);
            output.writeDouble(y);
            output.writeDouble(z);
            output.writeByte(yaw);
            output.writeByte(pitch);
        });
        break;
    }
    case play::PlayerPositionPacket::kId:
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeBytes(packet.body);
            // 1.19.3 is told whether this teleport takes the player off whatever they rode.
            protocol::writeBoolean(output, false);
        });
        break;
    case play::DamageEventPacket::kId: {
        const auto damage = decode(packet, [](io::ByteBuffer& in) { return play::DamageEventPacket::read(in); });
        if (!damage.has_value()) {
            return true;
        }
        packet.overrideId(kEntityEvent761);
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeInt(damage->entityId);
            output.writeByte(kHurtEvent);
        });
        break;
    }
    case play::HurtAnimationPacket::kId: {
        const auto hurt = decode(packet, [](io::ByteBuffer& in) { return play::HurtAnimationPacket::read(in); });
        if (!hurt.has_value()) {
            return true;
        }
        packet.overrideId(kAnimate761);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, hurt->entityId);
            output.writeByte(kHurtAnimation);
        });
        break;
    }
    case play::UpdateMobEffectPacket::kId: {
        auto effect = decode(packet, [](io::ByteBuffer& in) { return play::UpdateMobEffectPacket::read(in); });
        if (!effect.has_value() || effect->durationTicks >= 0) {
            return true;
        }
        // An effect that never ends is new in 1.19.4. A number this big is the same thing to anyone
        // watching it count down.
        effect->durationTicks = kPracticallyForever;
        encode(packet, [&](io::ByteBuffer& out) { effect->write(out); });
        break;
    }
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
