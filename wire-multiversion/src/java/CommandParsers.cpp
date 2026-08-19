#include <basalt/wire/multiversion/java/CommandParsers.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

namespace basalt::wire::multiversion::java::commandParsers {
namespace {

constexpr std::uint8_t kTypeMask = 0x03;
constexpr std::uint8_t kLiteral = 1;
constexpr std::uint8_t kArgument = 2;
constexpr std::uint8_t kRedirect = 0x08;
constexpr std::uint8_t kSuggestions = 0x10;
constexpr std::uint8_t kMinBit = 1;
constexpr std::uint8_t kMaxBit = 2;

constexpr std::int32_t kFloat = 1;
constexpr std::int32_t kDouble = 2;
constexpr std::int32_t kInteger = 3;
constexpr std::int32_t kLong = 4;
constexpr std::int32_t kString = 5;
constexpr std::int32_t kEntity = 6;
constexpr std::int32_t kScoreHolder = 30;
constexpr std::int32_t kTime = 42;
constexpr std::int32_t kFirstRegistry = 43;

void copyVarInt(io::ByteBuffer& in, io::ByteBuffer& out) { protocol::varInt::write(out, protocol::varInt::read(in)); }

void copyString(io::ByteBuffer& in, io::ByteBuffer& out) { protocol::writeString(out, protocol::readString(in)); }

void copyProperties(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t parser, std::int32_t lastRegistry) {
    if (parser == kFloat || parser == kInteger || parser == kDouble || parser == kLong) {
        const std::uint8_t flags = in.readByte();
        out.writeByte(flags);
        const bool wide = parser == kDouble || parser == kLong;
        for (const std::uint8_t bit : {kMinBit, kMaxBit}) {
            if ((flags & bit) == 0) {
                continue;
            }
            if (wide) {
                out.writeLong(in.readLong());
            } else {
                out.writeInt(in.readInt());
            }
        }
    } else if (parser == kString) {
        copyVarInt(in, out);
    } else if (parser == kEntity || parser == kScoreHolder) {
        out.writeByte(in.readByte());
    } else if (parser == kTime) {
        out.writeInt(in.readInt());
    } else if (parser >= kFirstRegistry && parser <= lastRegistry) {
        copyString(in, out);
    }
}

} // namespace

bool remap(MutablePacket& packet, std::int32_t lastRegistry, std::int32_t (*newId)(std::int32_t)) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    const std::int32_t nodes = protocol::readCollectionSize(in, 2);
    protocol::varInt::write(out, nodes);
    for (std::int32_t i = 0; i < nodes && !in.failed(); ++i) {
        const std::uint8_t flags = in.readByte();
        out.writeByte(flags);
        const std::int32_t children = protocol::readCollectionSize(in);
        protocol::varInt::write(out, children);
        for (std::int32_t child = 0; child < children && !in.failed(); ++child) {
            copyVarInt(in, out);
        }
        if ((flags & kRedirect) != 0) {
            copyVarInt(in, out);
        }
        const std::uint8_t type = flags & kTypeMask;
        if (type == kLiteral || type == kArgument) {
            copyString(in, out);
        }
        if (type != kArgument) {
            continue;
        }
        const std::int32_t parser = protocol::varInt::read(in);
        protocol::varInt::write(out, newId(parser));
        copyProperties(in, out, parser, lastRegistry);
        if ((flags & kSuggestions) != 0) {
            copyString(in, out);
        }
    }
    copyVarInt(in, out);
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java::commandParsers
