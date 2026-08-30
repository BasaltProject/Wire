#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/handshake/HandshakePacket.hpp>

namespace basalt::wire::protocol::handshake {

void HandshakePacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, protocolVersion);
    writeString(buffer, hostName);
    buffer.writeShort(static_cast<std::int16_t>(port));
    varInt::write(buffer, static_cast<std::int32_t>(intent));
}

std::optional<HandshakePacket> HandshakePacket::read(io::ByteBuffer& buffer, std::int32_t maxHostLength) {
    HandshakePacket packet;
    packet.protocolVersion = varInt::read(buffer);
    packet.hostName = readString(buffer, maxHostLength);
    packet.port = static_cast<std::uint16_t>(buffer.readShort());
    const std::optional<ClientIntent> intent = clientIntentFromId(varInt::read(buffer));
    if (buffer.failed() || !intent) {
        return std::nullopt;
    }
    packet.intent = *intent;
    return packet;
}

} // namespace basalt::wire::protocol::handshake
