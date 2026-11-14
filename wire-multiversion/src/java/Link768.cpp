#include <basalt/wire/multiversion/java/Link768.hpp>
#include <basalt/wire/multiversion/java/PlayerInfoActions.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/VehiclePackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

namespace play = protocol::play;

namespace {

/// parity: ServerboundPlayerInputPacket, numbered as this server does, and the Input flags it carries.
constexpr std::int32_t kPlayerInput = 0x2b;
constexpr std::uint8_t kForward = 1;
constexpr std::uint8_t kBackward = 2;
constexpr std::uint8_t kLeft = 4;
constexpr std::uint8_t kRight = 8;
constexpr std::uint8_t kJump = 16;
constexpr std::uint8_t kShift = 32;

/// parity: the 1.21.1 steering flags.
constexpr std::uint8_t kSteerJump = 1;
constexpr std::uint8_t kSteerDismount = 2;

constexpr std::int32_t kPressShift = 0;
constexpr std::int32_t kReleaseShift = 1;

} // namespace

bool Link768::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::PlayerInfoUpdatePacket::kId:
        // 1.21.4 added whether to show the hat layer.
        playerInfoActions::drop(packet, play::PlayerInfoUpdatePacket::kUpdateHat);
        break;
    case play::SetHeldSlotPacket::kId: {
        const auto slot = decode(packet, [](io::ByteBuffer& in) { return play::SetHeldSlotPacket::read(in); });
        if (!slot.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& out) { out.writeByte(static_cast<std::uint8_t>(slot->slot)); });
        break;
    }
    case play::LevelParticlesPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const bool overrideLimiter = input.readByte() != 0;
        static_cast<void>(input.readByte());
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::writeBoolean(output, overrideLimiter);
            output.writeBytes(input.readable());
        });
        break;
    }
    default:
        break;
    }
    return true;
}

bool Link768::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    if (packet.nativeId() == kPlayerInput) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const float sideways = input.readFloat();
        const float forward = input.readFloat();
        const std::uint8_t flags = input.readByte();
        if (input.failed()) {
            return false;
        }
        movement_ = static_cast<std::uint8_t>((forward > 0.0F ? kForward : 0) | (forward < 0.0F ? kBackward : 0) |
                                              (sideways > 0.0F ? kLeft : 0) | (sideways < 0.0F ? kRight : 0) |
                                              ((flags & kSteerJump) != 0 ? kJump : 0));
        const bool shift = sneaking_ || (flags & kSteerDismount) != 0;
        packet.body.assign(1, static_cast<std::uint8_t>(movement_ | (shift ? kShift : 0)));
        return true;
    }
    if (packet.nativeId() == play::ServerboundMoveVehiclePacket::kId) {
        // 1.21.4 added whether the vehicle stands on the ground.
        packet.body.push_back(0);
        return true;
    }
    if (packet.nativeId() != play::PlayerCommandPacket::kId) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    static_cast<void>(protocol::varInt::read(input));
    const std::int32_t action = protocol::varInt::read(input);
    if (input.failed() || (action != kPressShift && action != kReleaseShift)) {
        return true;
    }
    // Before 1.21.2 the shift key was a command; from then on it is one of the held inputs.
    sneaking_ = action == kPressShift;
    packet.id = kPlayerInput;
    packet.becomes(kPlayerInput);
    packet.body.assign(1, static_cast<std::uint8_t>(movement_ | (sneaking_ ? kShift : 0)));
    return true;
}

} // namespace basalt::wire::multiversion::java
