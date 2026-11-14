#include <basalt/wire/multiversion/java/Link338Up.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

constexpr std::int32_t kKeepAlive = 0x1F;
constexpr std::int32_t kKeepAliveReply = 0x0B;

} // namespace

bool Link338Up::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.id != kKeepAlive) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t id = protocol::varInt::read(in);
    if (in.failed()) {
        return false;
    }
    encode(packet, [id](io::ByteBuffer& out) { out.writeLong(id); });
    return true;
}

bool Link338Up::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.id != kKeepAliveReply) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int64_t id = in.readLong();
    if (in.failed()) {
        return false;
    }
    encode(packet, [id](io::ByteBuffer& out) { protocol::varInt::write(out, static_cast<std::int32_t>(id)); });
    return true;
}

} // namespace basalt::wire::multiversion::java
