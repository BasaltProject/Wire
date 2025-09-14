#include <basalt/wire/multiversion/java/Link109.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

namespace basalt::wire::multiversion::java {

bool Link109::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.nativeId() != protocol::play::MapChunkPacket::kId) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = input.readInt();
    const std::int32_t z = input.readInt();
    const std::uint8_t fullChunk = input.readByte();
    const std::int32_t mask = protocol::varInt::read(input);
    const std::int32_t length = protocol::varInt::read(input);
    const std::vector<std::uint8_t> data = input.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length));
    if (input.failed()) {
        return true;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(x);
        output.writeInt(z);
        output.writeByte(fullChunk);
        protocol::varInt::write(output, mask);
        protocol::varInt::write(output, static_cast<std::int32_t>(data.size()));
        output.writeBytes(data);
    });
    return true;
}

} // namespace basalt::wire::multiversion::java
