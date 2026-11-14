#include <basalt/wire/multiversion/java/SlotCopy.hpp>
#include <basalt/wire/multiversion/java/TradeLinks.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

using protocol::ProtocolState;

constexpr std::int32_t kTradeList = 0x27;

/// Rewrites the trades of a villager, calling `perTrade` after the fields they all share.
template <typename Tail, typename PerTrade>
bool rewriteTrades(MutablePacket& packet, PerTrade perTrade, Tail tail) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    protocol::varInt::write(out, protocol::varInt::read(in));
    const std::uint8_t trades = in.readByte();
    out.writeByte(trades);
    for (std::uint8_t index = 0; index < trades && !in.failed(); ++index) {
        copyFlatStack(in, out);
        copyFlatStack(in, out);
        const std::uint8_t second = in.readByte();
        out.writeByte(second);
        if (second != 0) {
            copyFlatStack(in, out);
        }
        out.writeBytes(in.readSpan(1 + 4 * 4 + 4));
        perTrade(out);
    }
    out.writeBytes(in.readable());
    tail(out);
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

} // namespace

bool Link480Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play || packet.id != kTradeList) {
        return true;
    }
    return rewriteTrades(
        packet, [](io::ByteBuffer&) {}, [](io::ByteBuffer& out) { protocol::writeBoolean(out, true); });
}

bool Link490Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play || packet.id != kTradeList) {
        return true;
    }
    return rewriteTrades(packet, [](io::ByteBuffer& out) { out.writeInt(0); }, [](io::ByteBuffer&) {});
}

} // namespace basalt::wire::multiversion::java
