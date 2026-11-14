#include <basalt/wire/multiversion/java/Link338.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>

namespace basalt::wire::multiversion::java {

namespace play = protocol::play;

bool Link338::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.nativeId() != play::PlayKeepAlivePacket::kClientboundId) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int64_t id = input.readLong();
    if (input.failed()) {
        return false;
    }
    keepAlives_[keepAliveNext_++ % keepAlives_.size()] = id;
    io::ByteBuffer output;
    protocol::varInt::write(output, static_cast<std::int32_t>(id));
    packet.body = output.toVector();
    return true;
}

bool Link338::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.nativeId() != play::PlayKeepAlivePacket::kServerboundId) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t id = protocol::varInt::read(input);
    const auto found =
        std::ranges::find_if(keepAlives_, [id](std::int64_t sent) { return static_cast<std::int32_t>(sent) == id; });
    if (input.failed() || found == keepAlives_.end()) {
        return false;
    }
    io::ByteBuffer output;
    output.writeLong(*found);
    packet.body = output.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
