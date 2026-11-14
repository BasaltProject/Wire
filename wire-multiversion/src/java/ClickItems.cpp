#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/IdMap.hpp>
#include <basalt/wire/multiversion/java/ClickItems.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

namespace basalt::wire::multiversion::java {

bool renumberClickItems(std::vector<std::uint8_t>& body, const ComposedIdMap& items) {
    constexpr std::int32_t kMaxChanged = 128;
    io::ByteBuffer in = io::ByteBuffer::wrap(body);
    io::ByteBuffer out;
    for (int field = 0; field < 2; ++field) {
        protocol::varInt::write(out, protocol::varInt::read(in));
    }
    out.writeShort(in.readShort());
    out.writeByte(in.readByte());
    protocol::varInt::write(out, protocol::varInt::read(in));
    const std::int32_t changed = protocol::readCollectionSize(in, 3);
    if (changed > kMaxChanged) {
        return false;
    }
    protocol::varInt::write(out, changed);
    for (std::int32_t index = 0; index <= changed && !in.failed(); ++index) {
        if (index < changed) {
            out.writeShort(in.readShort());
        }
        const bool present = protocol::readBoolean(in);
        protocol::writeBoolean(out, present);
        if (!present) {
            continue;
        }
        const std::int32_t item = items.unmap(protocol::varInt::read(in));
        protocol::varInt::write(out, item == IdMap::kNone ? 0 : item);
        protocol::varInt::write(out, protocol::varInt::read(in));
        const std::int32_t added = protocol::readCollectionSize(in, 5);
        protocol::varInt::write(out, added);
        for (std::int32_t component = 0; component < added && !in.failed(); ++component) {
            protocol::varInt::write(out, protocol::varInt::read(in));
            out.writeInt(in.readInt());
        }
        const std::int32_t removed = protocol::readCollectionSize(in, 1);
        protocol::varInt::write(out, removed);
        for (std::int32_t component = 0; component < removed && !in.failed(); ++component) {
            protocol::varInt::write(out, protocol::varInt::read(in));
        }
    }
    if (in.failed()) {
        return false;
    }
    body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
