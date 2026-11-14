#include <basalt/wire/multiversion/java/Link770.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

/// parity: the two shift actions that led ServerboundPlayerCommandPacket.Action until 1.21.6.
constexpr std::int32_t kShiftActions = 2;

} // namespace

bool Link770::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.nativeId() != protocol::play::PlayerCommandPacket::kId) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    const std::int32_t action = protocol::varInt::read(input);
    if (input.failed() || action < kShiftActions) {
        return false;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, entityId);
        protocol::varInt::write(output, action - kShiftActions);
        output.writeBytes(input.readable());
    });
    return true;
}

} // namespace basalt::wire::multiversion::java
