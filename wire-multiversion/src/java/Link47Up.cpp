#include <basalt/wire/multiversion/java/Content340.hpp>
#include <basalt/wire/multiversion/java/Flattening340.hpp>
#include <basalt/wire/multiversion/java/Link47Up.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
namespace flat = flattening340;

constexpr std::int32_t kLogin = 0x01;
constexpr std::int32_t kRespawn = 0x07;
constexpr std::int32_t kPlayerPosition = 0x08;
constexpr std::int32_t kAnimate = 0x0B;
constexpr std::int32_t kEffect = 0x1D;
constexpr std::int32_t kChunk = 0x21;
constexpr std::int32_t kChunkBulk = 0x26;
constexpr std::int32_t kNamedSound = 0x29;
constexpr std::int32_t kSetSlot = 0x2F;
constexpr std::int32_t kWindowItems = 0x30;
constexpr std::int32_t kMap = 0x34;
constexpr std::int32_t kTeams = 0x3E;
constexpr std::int32_t kCompression = 0x46;
constexpr std::int32_t kEntityNbt = 0x49;

constexpr std::int32_t kInteract = 0x02;
constexpr std::int32_t kPlayerAction = 0x07;
constexpr std::int32_t kUseItemOn = 0x08;
constexpr std::int32_t kSwing = 0x0A;
constexpr std::int32_t kPlayerCommand = 0x0B;
constexpr std::int32_t kClick = 0x0E;
constexpr std::int32_t kCreativeSlot = 0x10;
constexpr std::int32_t kSignUpdate = 0x12;
constexpr std::int32_t kSuggestion = 0x14;
constexpr std::int32_t kClientSettings = 0x15;
constexpr std::int32_t kUseItem19 = 0x1D;

constexpr std::int32_t kOverworld = 0;
constexpr std::uint8_t kEatAnimation = 3;
constexpr std::uint8_t kShowParticles = 0x02;
constexpr std::int32_t kSwapHands = 6;
constexpr std::int32_t kMainHand = 0;
constexpr std::int32_t kInteractAt = 2;
constexpr std::int32_t kAttack = 1;
constexpr std::int32_t kSignLines = 4;

/// The 1.9 category a renamed sound belongs to, as 1.8 never sent one.
[[nodiscard]] std::int32_t categoryOf(std::string_view name) {
    constexpr std::array<std::pair<std::string_view, std::int32_t>, 9> kPrefixes = {{
        {"music.", 1},
        {"record.", 2},
        {"weather.", 3},
        {"block.", 4},
        {"entity.player.", 7},
        {"entity.generic.", 6},
        {"entity.", 6},
        {"ambient.", 8},
        {"ui.", 0},
    }};
    for (const auto& [prefix, category] : kPrefixes) {
        if (name.starts_with(prefix)) {
            return category;
        }
    }
    return 0;
}

} // namespace

bool Link47Up::small(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kLogin: {
        out.writeInt(in.readInt());
        out.writeByte(in.readByte());
        const std::int8_t dimension = static_cast<std::int8_t>(in.readByte());
        skyLight_ = dimension == kOverworld;
        out.writeByte(static_cast<std::uint8_t>(dimension));
        break;
    }
    case kRespawn: {
        const std::int32_t dimension = in.readInt();
        skyLight_ = dimension == kOverworld;
        kinds_.clear();
        vehicles_.clear();
        out.writeInt(dimension);
        break;
    }
    case kPlayerPosition:
        out.writeBytes(in.readable());
        pv::varInt::write(out, ++teleports_);
        packet.body = out.toVector();
        return true;
    case kAnimate:
        pv::varInt::write(out, pv::varInt::read(in));
        if (in.readByte() == kEatAnimation) {
            return false;
        }
        return true;
    case kEffect:
        if (packet.body.empty()) {
            return false;
        }
        packet.body.back() = packet.body.back() != 0 ? 0 : kShowParticles;
        return true;
    case kNamedSound: {
        const std::string name = pv::readString(in);
        const auto renames = flat::sounds18();
        const auto found = std::ranges::lower_bound(renames, std::string_view{name}, {}, &flat::Rename::from);
        const std::string_view renamed =
            found != renames.end() && found->from == name ? found->to : std::string_view{name};
        pv::writeString(out, renamed);
        pv::varInt::write(out, categoryOf(renamed));
        break;
    }
    case kSetSlot:
        out.writeByte(in.readByte());
        out.writeShort(in.readShort());
        content340::upLegacyStack(in, out);
        break;
    case kWindowItems: {
        out.writeByte(in.readByte());
        const std::int16_t count = in.readShort();
        out.writeShort(count);
        for (std::int16_t slot = 0; slot < count && !in.failed(); ++slot) {
            content340::upLegacyStack(in, out);
        }
        break;
    }
    case kMap:
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeByte(in.readByte());
        pv::writeBoolean(out, true);
        break;
    case kTeams: {
        pv::writeString(out, pv::readString(in));
        const std::uint8_t mode = in.readByte();
        out.writeByte(mode);
        if (mode == 0 || mode == 2) {
            for (int field = 0; field < 3; ++field) {
                pv::writeString(out, pv::readString(in));
            }
            out.writeByte(in.readByte());
            pv::writeString(out, pv::readString(in));
            pv::writeString(out, "always");
        }
        break;
    }
    case kCompression:
    case kEntityNbt:
        return false;
    default:
        return entities(packet);
    }
    if (in.failed()) {
        return false;
    }
    out.writeBytes(in.readable());
    packet.body = out.toVector();
    return true;
}

bool Link47Up::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    if (packet.id == kChunk) {
        return chunk(packet);
    }
    if (packet.id == kChunkBulk) {
        return chunkBulk(packet);
    }
    return small(packet);
}

bool Link47Up::serverboundForeign(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.id != kUseItem19) {
        return false;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    if (pv::varInt::read(in) != kMainHand || in.failed()) {
        return false;
    }
    constexpr std::uint8_t kNoFace = 0xFF;
    io::ByteBuffer out;
    out.writeLong(-1);
    out.writeByte(kNoFace);
    out.writeShort(-1);
    out.writeBytes(std::array<std::uint8_t, 3>{});
    packet.id = kUseItemOn;
    packet.body = out.toVector();
    return true;
}

bool Link47Up::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kUseItemOn: {
        // A use of the item in hand, already written for 1.8 by serverboundForeign.
        if (packet.body.size() == 8 + 1 + 2 + 3 && packet.body[8] == 0xFF) {
            return true;
        }
        out.writeLong(in.readLong());
        out.writeByte(static_cast<std::uint8_t>(pv::varInt::read(in)));
        if (pv::varInt::read(in) != kMainHand) {
            return false;
        }
        out.writeShort(-1);
        out.writeBytes(in.readSpan(3));
        break;
    }
    case kSuggestion: {
        pv::writeString(out, pv::readString(in));
        static_cast<void>(in.readByte());
        out.writeBytes(in.readable());
        break;
    }
    case kClientSettings:
        pv::writeString(out, pv::readString(in));
        out.writeByte(in.readByte());
        out.writeByte(static_cast<std::uint8_t>(pv::varInt::read(in)));
        out.writeByte(in.readByte());
        out.writeByte(in.readByte());
        static_cast<void>(pv::varInt::read(in));
        break;
    case kInteract: {
        const std::int32_t target = pv::varInt::read(in);
        const std::int32_t type = pv::varInt::read(in);
        pv::varInt::write(out, target);
        pv::varInt::write(out, type);
        if (type == kInteractAt) {
            out.writeBytes(in.readSpan(12));
        }
        if (type != kAttack && pv::varInt::read(in) != kMainHand) {
            return false;
        }
        packet.body = out.toVector();
        return !in.failed();
    }
    case kPlayerAction:
        return packet.body.empty() || packet.body[0] != kSwapHands;
    case kPlayerCommand: {
        pv::varInt::write(out, pv::varInt::read(in));
        const std::int32_t action = pv::varInt::read(in);
        if (action == 6 || action == 8) {
            return false;
        }
        pv::varInt::write(out, action == 7 ? 6 : action);
        break;
    }
    case kSwing:
        if (pv::varInt::read(in) != kMainHand) {
            return false;
        }
        packet.body.clear();
        return true;
    case kSignUpdate:
        out.writeLong(in.readLong());
        for (int line = 0; line < kSignLines; ++line) {
            pv::writeString(out, content340::legacyToJson(pv::readString(in)));
        }
        break;
    case kCreativeSlot:
        out.writeShort(in.readShort());
        content340::downLegacyStack(in, out);
        break;
    case kClick:
        out.writeBytes(in.readSpan(1 + 2 + 1 + 2 + 1));
        content340::downLegacyStack(in, out);
        break;
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
