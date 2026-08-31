#include <basalt/wire/multiversion/java/PlayerSpawn.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

namespace basalt::wire::multiversion::java {

void playerAsEntity(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t playerType) {
    protocol::varInt::write(out, protocol::varInt::read(in));
    out.writeBytes(in.readSpan(16));
    protocol::varInt::write(out, playerType);
    out.writeBytes(in.readSpan(24));
    const std::uint8_t yaw = in.readByte();
    const std::uint8_t pitch = in.readByte();
    out.writeByte(pitch);
    out.writeByte(yaw);
    out.writeByte(yaw);
    protocol::varInt::write(out, 0);
    out.writeShort(0);
    out.writeShort(0);
    out.writeShort(0);
}

} // namespace basalt::wire::multiversion::java
