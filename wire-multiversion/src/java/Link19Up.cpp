#include <basalt/wire/multiversion/java/Link19Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cmath>
#include <cstdint>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;

constexpr std::int32_t kChunk = 0x20;
constexpr std::int32_t kJoinGame = 0x23;
constexpr std::int32_t kUpdateSign = 0x46;
constexpr std::int32_t kSound = 0x47;
constexpr std::int32_t kBlockEntity194 = 0x09;

constexpr std::uint8_t kSignAction = 9;
constexpr std::int32_t kSignLines = 4;
constexpr std::int32_t kFirstShiftedSound = 415;
constexpr float kOldPitchScale = 63.5F;
constexpr float kPitchScale = 63.0F;

} // namespace

bool Link109Up::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kChunk:
        out.writeBytes(in.readable());
        pv::varInt::write(out, 0);
        packet.body = out.toVector();
        return true;
    case kSound:
        out.writeBytes(in.readSpan(in.readableBytes() - 1));
        out.writeByte(static_cast<std::uint8_t>(std::lround(in.readByte() / kOldPitchScale * kPitchScale)));
        break;
    case kUpdateSign: {
        const std::int64_t position = in.readLong();
        nbt::NbtCompound sign;
        sign.putString("id", "Sign");
        sign.putInt("x", static_cast<std::int32_t>(position >> 38));
        sign.putInt("y", static_cast<std::int32_t>((position >> 26) & 0xFFF));
        sign.putInt("z", static_cast<std::int32_t>((position << 38) >> 38));
        for (std::int32_t line = 1; line <= kSignLines; ++line) {
            sign.putString("Text" + std::to_string(line), pv::readString(in));
        }
        out.writeLong(position);
        out.writeByte(kSignAction);
        writeNamedNbt(out, nbt::NbtTag::ofCompound(std::move(sign)));
        packet.overrideId(kBlockEntity194);
        break;
    }
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

bool Link107Up::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kJoinGame:
        out.writeInt(in.readInt());
        out.writeByte(in.readByte());
        out.writeInt(static_cast<std::int8_t>(in.readByte()));
        break;
    case kSound: {
        const std::int32_t sound = pv::varInt::read(in);
        pv::varInt::write(out, sound >= kFirstShiftedSound ? sound + 1 : sound);
        break;
    }
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    out.writeBytes(in.readable());
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
