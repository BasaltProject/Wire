#include <basalt/wire/multiversion/java/Content340.hpp>
#include <basalt/wire/multiversion/java/Flattening340.hpp>
#include <basalt/wire/multiversion/java/Link110Up.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;

constexpr std::int32_t kSpawnObject = 0x00;
constexpr std::int32_t kSpawnMob = 0x03;
constexpr std::int32_t kSpawnPlayer = 0x05;
constexpr std::int32_t kNamedSound = 0x19;
constexpr std::int32_t kDestroyEntities = 0x30;
constexpr std::int32_t kResourcePack = 0x32;
constexpr std::int32_t kEntityData = 0x39;
constexpr std::int32_t kSound = 0x46;
constexpr std::int32_t kPackStatus = 0x16;

constexpr std::int32_t kPotion = 73;
constexpr std::uint8_t kNoGravity = 5;
constexpr std::uint8_t kEnd = 0xFF;
constexpr float kPitchScale = 63.0F;

/// parity: 1.10 put the no-gravity flag at 5, so everything from there moves up one; 1.9 already put a potion at 6.
[[nodiscard]] bool shiftEntityData(io::ByteBuffer& in, io::ByteBuffer& out, bool potion) {
    while (!in.failed()) {
        const std::uint8_t index = in.readByte();
        if (index == kEnd) {
            break;
        }
        const std::int32_t type = in.readByte();
        io::ByteBuffer value;
        if (!content340::copyEntityValue(in, value, type)) {
            return false;
        }
        out.writeByte(index >= kNoGravity && !potion ? index + 1 : index);
        out.writeByte(static_cast<std::uint8_t>(type));
        out.writeBytes(value.readable());
    }
    out.writeByte(kEnd);
    return !in.failed();
}

void writePitch(io::ByteBuffer& in, io::ByteBuffer& out) {
    out.writeBytes(in.readSpan(4 + 4 + 4 + 4));
    out.writeFloat(static_cast<float>(in.readByte()) / kPitchScale);
}

} // namespace

bool Link110Up::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kSpawnObject: {
        const std::int32_t entity = pv::varInt::read(in);
        static_cast<void>(in.readSpan(16));
        if (in.readByte() == kPotion) {
            potions_.insert(entity);
        }
        return true;
    }
    case kSpawnMob:
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeBytes(in.readSpan(16 + 1 + 24 + 3 + 6));
        if (!shiftEntityData(in, out, false)) {
            return false;
        }
        break;
    case kSpawnPlayer:
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeBytes(in.readSpan(16 + 24 + 2));
        if (!shiftEntityData(in, out, false)) {
            return false;
        }
        break;
    case kEntityData: {
        const std::int32_t entity = pv::varInt::read(in);
        pv::varInt::write(out, entity);
        if (!shiftEntityData(in, out, potions_.contains(entity))) {
            return false;
        }
        break;
    }
    case kDestroyEntities: {
        const std::int32_t count = pv::readCollectionSize(in);
        for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
            potions_.erase(pv::varInt::read(in));
        }
        return true;
    }
    case kSound: {
        const std::int32_t sound = flattening340::sounds110().map(pv::varInt::read(in));
        if (sound == IdMap::kNone) {
            return false;
        }
        pv::varInt::write(out, sound);
        pv::varInt::write(out, pv::varInt::read(in));
        writePitch(in, out);
        break;
    }
    case kNamedSound:
        pv::writeString(out, pv::readString(in));
        pv::varInt::write(out, pv::varInt::read(in));
        writePitch(in, out);
        break;
    case kResourcePack:
        static_cast<void>(pv::readString(in));
        packHash_ = pv::readString(in);
        return true;
    default:
        return true;
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

bool Link110Up::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.id != kPackStatus) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t result = pv::varInt::read(in);
    if (in.failed()) {
        return false;
    }
    io::ByteBuffer out;
    pv::writeString(out, packHash_);
    pv::varInt::write(out, result);
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
