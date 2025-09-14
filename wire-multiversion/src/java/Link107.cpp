#include <basalt/wire/multiversion/java/Link107.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

namespace basalt::wire::multiversion::java {

bool Link107::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.nativeId() != protocol::play::LoginPlayPacket::kId) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = input.readInt();
    const std::uint8_t gamemode = input.readByte();
    const std::int32_t dimension = input.readInt();
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(entityId);
        output.writeByte(gamemode);
        output.writeByte(static_cast<std::uint8_t>(dimension));
        output.writeBytes(input.readable());
    });
    return true;
}

} // namespace basalt::wire::multiversion::java
