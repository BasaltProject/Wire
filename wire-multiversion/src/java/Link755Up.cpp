#include <basalt/wire/multiversion/java/Link755Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kContainerSetContent = 20;
constexpr std::int32_t kContainerSetSlot = 22;
constexpr std::int32_t kRemoveEntity = 58;
constexpr std::int32_t kContainerClick = 8;
constexpr std::int32_t kEditBook = 11;

/// parity: `Items.WRITABLE_BOOK` in 1.17.
constexpr std::int32_t kWritableBook = 942;
constexpr std::size_t kMaxBookPages = 200;

void copyStack(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::uint8_t present = in.readByte();
    out.writeByte(present);
    if (present == 0) {
        return;
    }
    pv::varInt::write(out, pv::varInt::read(in));
    out.writeByte(in.readByte());
    const auto tag = readNamedNbt(in);
    if (!in.failed()) {
        out.writeBytes(tag->raw);
    }
}

} // namespace

bool Link755Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kRemoveEntity:
        pv::varInt::write(out, 1);
        out.writeBytes(in.readable());
        break;
    case kContainerSetSlot:
        out.writeByte(in.readByte());
        pv::varInt::write(out, 0);
        out.writeBytes(in.readable());
        break;
    case kContainerSetContent: {
        out.writeByte(in.readByte());
        pv::varInt::write(out, 0);
        const auto count = static_cast<std::uint16_t>(in.readShort());
        pv::varInt::write(out, count);
        for (std::uint16_t index = 0; index < count && !in.failed(); ++index) {
            copyStack(in, out);
        }
        out.writeByte(0);
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

bool Link755Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    if (packet.id == kContainerClick) {
        out.writeByte(in.readByte());
        static_cast<void>(pv::varInt::read(in));
        out.writeBytes(in.readable());
    } else if (packet.id == kEditBook) {
        const std::int32_t slot = pv::varInt::read(in);
        const std::int32_t pages = pv::readCollectionSize(in, 1);
        nbt::NbtList list{nbt::NbtType::String, {}};
        for (std::int32_t index = 0; index < pages && !in.failed(); ++index) {
            std::string page = pv::readString(in);
            if (list.size() < kMaxBookPages) {
                list.push(nbt::NbtTag::ofString(std::move(page)));
            }
        }
        if (list.empty()) {
            list.push(nbt::NbtTag::ofString(""));
        }
        const bool signing = pv::readBoolean(in);
        nbt::NbtCompound book;
        book.set("pages", nbt::NbtTag::ofList(std::move(list)));
        if (signing) {
            book.putString("title", pv::readString(in));
            book.putString("author", "");
        }
        if (in.failed()) {
            return false;
        }
        out.writeByte(1);
        pv::varInt::write(out, kWritableBook);
        out.writeByte(1);
        writeNamedNbt(out, nbt::NbtTag::ofCompound(std::move(book)));
        pv::writeBoolean(out, signing);
        pv::varInt::write(out, slot);
    } else {
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
