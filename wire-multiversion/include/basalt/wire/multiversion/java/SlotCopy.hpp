#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

namespace basalt::wire::multiversion::java {

/// Copies an item stack in the 1.13.2 to 1.16.5 layout: presence, id, count and a network NBT.
inline void copyFlatStack(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::uint8_t present = in.readByte();
    out.writeByte(present);
    if (present == 0) {
        return;
    }
    protocol::varInt::write(out, protocol::varInt::read(in));
    out.writeByte(in.readByte());
    const auto tag = readNamedNbt(in);
    if (!in.failed()) {
        out.writeBytes(tag->raw);
    }
}

} // namespace basalt::wire::multiversion::java
