#include <basalt/wire/multiversion/java/Link768.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

namespace play = protocol::play;

bool Link768::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
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

} // namespace basalt::wire::multiversion::java
