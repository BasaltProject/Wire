#include <basalt/wire/multiversion/java/Content340.hpp>
#include <basalt/wire/multiversion/java/Link316Up.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;

constexpr std::int32_t kSpawnMob = 0x03;
constexpr std::int32_t kDestroyEntities = 0x30;
constexpr std::int32_t kEntityData = 0x39;
constexpr std::int32_t kSound = 0x46;
constexpr std::int32_t kClientSettings = 0x04;

constexpr std::int32_t kEvoker = 34;
constexpr std::int32_t kVindicator = 36;
constexpr std::uint8_t kFirstMovedIndex = 12;
constexpr std::uint8_t kEnd = 0xFF;
constexpr std::size_t kLocaleLength = 7;

} // namespace

bool Link316Up::entityData(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t type) const {
    const bool illager = type == kEvoker || type == kVindicator;
    while (!in.failed()) {
        const std::uint8_t index = in.readByte();
        if (index == kEnd) {
            break;
        }
        const std::int32_t kind = in.readByte();
        out.writeByte(illager && index >= kFirstMovedIndex ? index + 1 : index);
        out.writeByte(static_cast<std::uint8_t>(kind));
        if (!content340::copyEntityValue(in, out, kind)) {
            return false;
        }
    }
    out.writeByte(kEnd);
    return !in.failed();
}

bool Link316Up::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kSound: {
        const std::int32_t sound = tables().soundEvent.map(pv::varInt::read(in));
        if (sound == IdMap::kNone) {
            return false;
        }
        pv::varInt::write(out, sound);
        out.writeBytes(in.readable());
        break;
    }
    case kSpawnMob: {
        const std::int32_t entity = pv::varInt::read(in);
        pv::varInt::write(out, entity);
        out.writeBytes(in.readSpan(16));
        const std::int32_t type = pv::varInt::read(in);
        pv::varInt::write(out, type);
        out.writeBytes(in.readSpan(24 + 3 + 6));
        types_[entity] = type;
        if (!entityData(in, out, type)) {
            return false;
        }
        break;
    }
    case kEntityData: {
        const std::int32_t entity = pv::varInt::read(in);
        pv::varInt::write(out, entity);
        const auto type = types_.find(entity);
        if (!entityData(in, out, type == types_.end() ? -1 : type->second)) {
            return false;
        }
        break;
    }
    case kDestroyEntities: {
        const std::int32_t count = pv::readCollectionSize(in);
        for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
            types_.erase(pv::varInt::read(in));
        }
        return true;
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

bool Link316Up::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.id != kClientSettings) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    std::string locale = pv::readString(in);
    if (in.failed()) {
        return false;
    }
    locale.resize(std::min(locale.size(), kLocaleLength));
    io::ByteBuffer out;
    pv::writeString(out, locale);
    out.writeBytes(in.readable());
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
