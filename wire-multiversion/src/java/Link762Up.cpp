#include <basalt/wire/multiversion/java/Link762Up.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kTileEntityData = 8;
constexpr std::int32_t kMapChunk = 36;
constexpr std::int32_t kUpdateLight = 39;
constexpr std::int32_t kLogin = 40;
constexpr std::int32_t kOpenSignEditor = 49;
constexpr std::int32_t kDeathCombat = 56;
constexpr std::int32_t kRespawn = 65;
constexpr std::int32_t kMultiBlockChange = 67;
constexpr std::int32_t kUpdateSign = 46;

constexpr std::string_view kEmptyLine = "{\"text\":\"\"}";

void copyVarInt(io::ByteBuffer& in, io::ByteBuffer& out) { pv::varInt::write(out, pv::varInt::read(in)); }

void copyBytes(io::ByteBuffer& in, io::ByteBuffer& out, std::size_t count) { out.writeBytes(in.readSpan(count)); }

void copyRest(io::ByteBuffer& in, io::ByteBuffer& out) { out.writeBytes(in.readable()); }

nbt::NbtCompound sideOf(std::vector<std::string> lines, std::string color, bool glowing) {
    nbt::NbtList messages;
    for (std::string& line : lines) {
        messages.push(nbt::NbtTag::ofString(std::move(line)));
    }
    nbt::NbtCompound side;
    side.set("messages", nbt::NbtTag::ofList(std::move(messages)));
    side.putString("color", std::move(color));
    side.putByte("has_glowing_text", glowing ? 1 : 0);
    return side;
}

void signToTwoSides(nbt::NbtCompound& entity) {
    if (!entity.contains("Text1")) {
        return;
    }
    std::vector<std::string> front;
    for (int line = 1; line <= 4; ++line) {
        const std::string key = "Text" + std::to_string(line);
        front.emplace_back(entity.getString(key, kEmptyLine));
        entity.remove(key);
    }
    const std::string color{entity.getString("Color", "black")};
    const bool glowing = entity.getBoolean("GlowingText");
    entity.remove("Color");
    entity.remove("GlowingText");
    entity.set("front_text", nbt::NbtTag::ofCompound(sideOf(std::move(front), color, glowing)));
    entity.set("back_text",
               nbt::NbtTag::ofCompound(sideOf(std::vector<std::string>(4, std::string{kEmptyLine}), "black", false)));
    entity.putByte("is_waxed", 0);
}

/// A named-root NBT value that is a block entity: signs change, everything else is copied.
void convertBlockEntity(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::uint8_t type = in.readByte();
    if (in.failed()) {
        return;
    }
    if (type == 0) {
        out.writeByte(0);
        return;
    }
    const std::int32_t length = static_cast<std::uint16_t>(in.readShort());
    in.skip(static_cast<std::size_t>(length));
    if (in.failed()) {
        return;
    }
    std::vector<std::uint8_t> window;
    window.push_back(type);
    const auto rest = in.readable();
    window.insert(window.end(), rest.begin(), rest.end());
    auto parsed = nbt::nbtIO::readAnonymous(window);
    if (!parsed) {
        in.markFailed();
        return;
    }
    in.skip(parsed->bytesConsumed - 1);
    if (auto* compound = parsed->tag.get<nbt::NbtCompound>()) {
        signToTwoSides(*compound);
    }
    out.writeBytes(nbt::nbtIO::write(parsed->tag, ""));
}

template <typename Block>
void transform(MutablePacket& packet, Block&& block) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    std::forward<Block>(block)(in, out);
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteChunk(io::ByteBuffer& in, io::ByteBuffer& out) {
    copyBytes(in, out, 8);
    {
        const std::uint8_t type = in.readByte();
        out.writeByte(type);
        if (type != 0) {
            const std::int32_t length = static_cast<std::uint16_t>(in.readShort());
            out.writeShort(static_cast<std::int16_t>(length));
            copyBytes(in, out, static_cast<std::size_t>(length));
            std::vector<std::uint8_t> window;
            window.push_back(type);
            const auto rest = in.readable();
            window.insert(window.end(), rest.begin(), rest.end());
            const auto parsed = nbt::nbtIO::readAnonymous(window);
            if (!parsed) {
                in.markFailed();
                return;
            }
            copyBytes(in, out, parsed->bytesConsumed - 1);
        }
    }
    const std::int32_t size = pv::varInt::read(in);
    pv::varInt::write(out, size);
    if (size < 0) {
        in.markFailed();
        return;
    }
    copyBytes(in, out, static_cast<std::size_t>(size));
    const std::int32_t entities = pv::readCollectionSize(in, 4);
    pv::varInt::write(out, entities);
    for (std::int32_t index = 0; index < entities && !in.failed(); ++index) {
        copyBytes(in, out, 3);
        copyVarInt(in, out);
        convertBlockEntity(in, out);
    }
    static_cast<void>(in.readByte());
    copyRest(in, out);
}

} // namespace

bool Link762Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kMapChunk:
        transform(packet, rewriteChunk);
        break;
    case kUpdateLight:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            copyVarInt(in, out);
            static_cast<void>(in.readByte());
            copyRest(in, out);
        });
        break;
    case kMultiBlockChange:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyBytes(in, out, 8);
            static_cast<void>(in.readByte());
            copyRest(in, out);
        });
        break;
    case kOpenSignEditor:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyBytes(in, out, 8);
            pv::writeBoolean(out, true);
        });
        break;
    case kDeathCombat:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyVarInt(in, out);
            static_cast<void>(in.readInt());
            copyRest(in, out);
        });
        break;
    case kLogin:
    case kRespawn:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyRest(in, out);
            pv::varInt::write(out, 0);
        });
        break;
    case kTileEntityData:
        transform(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            copyBytes(in, out, 8);
            copyVarInt(in, out);
            convertBlockEntity(in, out);
        });
        break;
    default:
        break;
    }
    return true;
}

bool Link762Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play || packet.id != kUpdateSign) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    copyBytes(in, out, 8);
    const bool front = pv::readBoolean(in);
    copyRest(in, out);
    if (in.failed() || !front) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
