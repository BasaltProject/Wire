#include <basalt/wire/multiversion/java/Content340.hpp>
#include <basalt/wire/multiversion/java/Flattening340.hpp>
#include <basalt/wire/multiversion/java/Link340Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
namespace flat = flattening340;

constexpr std::int32_t kSpawnObject = 0x00;
constexpr std::int32_t kSpawnMob = 0x03;
constexpr std::int32_t kSpawnPainting = 0x04;
constexpr std::int32_t kSpawnPlayer = 0x05;
constexpr std::int32_t kDestroyEntities = 0x32;
constexpr std::int32_t kEntityData = 0x3C;

constexpr std::int32_t kObject = 1000;
constexpr std::int32_t kPlayer = -2;
constexpr std::int32_t kAreaEffectCloud = kObject + 3;
constexpr std::int32_t kMinecart = kObject + 10;
constexpr std::int32_t kArrow = kObject + 60;
constexpr std::int32_t kFallingBlock = 70;
constexpr std::int32_t kItemFrame = 71;
constexpr std::int32_t kWolf = 95;
constexpr std::array<std::int32_t, 4> kZombies = {23, 27, 54, 57};

constexpr std::uint8_t kEnd = 0xFF;
constexpr std::int32_t kTypeString = 3;
constexpr std::int32_t kTypeText = 4;
constexpr std::int32_t kTypeSlot = 5;
constexpr std::int32_t kTypeBlock = 12;
constexpr std::int32_t kOptionalText113 = 5;
constexpr std::int32_t kParticle113 = 15;
constexpr std::int32_t kSwimming = 0x10;
constexpr std::int32_t kWhite = 15;

constexpr std::array<std::string_view, 26> kMotives = {
    "Kebab",         "Aztec",  "Alban",    "Aztec2",   "Bomb",     "Plant",        "Wasteland", "Pool",      "Courbet",
    "Sea",           "Sunset", "Creebet",  "Wanderer", "Graham",   "Match",        "Bust",      "Stage",     "Void",
    "SkullAndRoses", "Wither", "Fighters", "Pointer",  "Pigscene", "BurningSkull", "Skeleton",  "DonkeyKong"};

/// Copies one 1.12 entity data value of `type`; false where the type is not one 1.12 has.
[[nodiscard]] bool copyValue(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t type) {
    switch (type) {
    case 0:
    case 6:
        out.writeByte(in.readByte());
        return true;
    case 1:
    case 10:
        pv::varInt::write(out, pv::varInt::read(in));
        return true;
    case 2:
        out.writeFloat(in.readFloat());
        return true;
    case kTypeString:
    case kTypeText:
        pv::writeString(out, pv::readString(in));
        return true;
    case kTypeSlot:
        content340::upStack(in, out);
        return true;
    case 7:
        out.writeBytes(in.readSpan(12));
        return true;
    case 8:
        out.writeLong(in.readLong());
        return true;
    case 9:
    case 11: {
        const bool present = pv::readBoolean(in);
        pv::writeBoolean(out, present);
        if (present) {
            out.writeBytes(in.readSpan(type == 9 ? 8 : 16));
        }
        return true;
    }
    case kTypeBlock: {
        const std::int32_t value = pv::varInt::read(in);
        pv::varInt::write(out, value == 0 ? 0 : content340::blockStateOfStateId(value));
        return true;
    }
    case 13: {
        const auto tag = readNamedNbt(in);
        if (tag) {
            out.writeBytes(tag->raw);
        }
        return true;
    }
    default:
        return false;
    }
}

} // namespace

bool Link340Up::entityData(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t kind) const {
    const bool zombie = std::ranges::find(kZombies, kind) != kZombies.end();
    std::array<std::int32_t, 3> cloud{-1, 0, 0};
    while (!in.failed()) {
        const std::uint8_t index = in.readByte();
        if (index == kEnd) {
            break;
        }
        const std::int32_t type = in.readByte();
        if (kind == kAreaEffectCloud && index >= 9 && index <= 11 && type == 1) {
            cloud[static_cast<std::size_t>(index - 9)] = pv::varInt::read(in);
            continue;
        }
        if (index == 2 && type == kTypeString) {
            const std::string name = pv::readString(in);
            out.writeByte(index);
            out.writeByte(kOptionalText113);
            pv::writeBoolean(out, !name.empty());
            if (!name.empty()) {
                pv::writeString(out, content340::legacyToJson(name));
            }
            continue;
        }
        // parity: AbstractArrow of 1.13 put the owner's UUID at 7, before the colour of the tipped arrow.
        const bool after = (zombie && index >= 15) || (kind == kArrow && index == 7);
        const std::uint8_t moved = after ? index + 1 : index;
        out.writeByte(moved);
        out.writeByte(static_cast<std::uint8_t>(type > kTypeText ? type + 1 : type));
        if (index == 0 && type == 0) {
            out.writeByte(in.readByte() & ~kSwimming);
        } else if (kind == kWolf && index == 17 && type == 1) {
            pv::varInt::write(out, kWhite - pv::varInt::read(in));
        } else if (kind == kMinecart && index == 9 && type == 1) {
            pv::varInt::write(out, content340::blockStateOfStateId(pv::varInt::read(in)));
        } else if (!copyValue(in, out, type)) {
            return false;
        }
    }
    if (cloud[0] >= 0) {
        out.writeByte(9);
        out.writeByte(kParticle113);
        const std::int32_t particle = content340::particleId(cloud[0]);
        pv::varInt::write(out, particle < 0 ? content340::particleId(0) : particle);
        if (particle >= 0) {
            constexpr std::array<float, 4> kRed{1.0F, 0.0F, 0.0F, 1.0F};
            content340::particleData(out, cloud[0], std::span{cloud}.subspan(1), kRed);
        }
    }
    out.writeByte(kEnd);
    return !in.failed();
}

bool Link340Up::entities(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kSpawnObject: {
        const std::int32_t entity = pv::varInt::read(in);
        pv::varInt::write(out, entity);
        out.writeBytes(in.readSpan(16));
        const std::int32_t type = in.readByte();
        out.writeByte(static_cast<std::uint8_t>(type));
        out.writeBytes(in.readSpan(24 + 2));
        std::int32_t data = in.readInt();
        if (type == kFallingBlock) {
            data = content340::blockStateOfStateId(data);
        } else if (type == kItemFrame) {
            constexpr std::array<std::int32_t, 4> kFacing = {3, 4, 2, 5};
            data = data >= 0 && data < 4 ? kFacing[static_cast<std::size_t>(data)] : data;
        }
        out.writeInt(data);
        out.writeBytes(in.readable());
        kinds_[entity] = kObject + type;
        break;
    }
    case kSpawnMob: {
        const std::int32_t entity = pv::varInt::read(in);
        pv::varInt::write(out, entity);
        out.writeBytes(in.readSpan(16));
        const std::int32_t legacy = pv::varInt::read(in);
        const std::int32_t type = flat::mobs().map(legacy);
        if (type == IdMap::kNone) {
            return false;
        }
        pv::varInt::write(out, type);
        out.writeBytes(in.readSpan(24 + 3 + 6));
        kinds_[entity] = legacy;
        if (!entityData(in, out, legacy)) {
            return false;
        }
        break;
    }
    case kSpawnPainting: {
        pv::varInt::write(out, pv::varInt::read(in));
        out.writeBytes(in.readSpan(16));
        const auto motive = std::ranges::find(kMotives, pv::readString(in));
        pv::varInt::write(out, motive == kMotives.end() ? 0 : static_cast<std::int32_t>(motive - kMotives.begin()));
        out.writeBytes(in.readable());
        break;
    }
    case kSpawnPlayer: {
        const std::int32_t entity = pv::varInt::read(in);
        pv::varInt::write(out, entity);
        out.writeBytes(in.readSpan(16 + 24 + 2));
        kinds_[entity] = kPlayer;
        if (!entityData(in, out, kPlayer)) {
            return false;
        }
        break;
    }
    case kEntityData: {
        const std::int32_t entity = pv::varInt::read(in);
        pv::varInt::write(out, entity);
        const auto kind = kinds_.find(entity);
        if (!entityData(in, out, kind == kinds_.end() ? kPlayer : kind->second)) {
            return false;
        }
        break;
    }
    case kDestroyEntities: {
        const std::int32_t count = pv::readCollectionSize(in);
        for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
            kinds_.erase(pv::varInt::read(in));
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

} // namespace basalt::wire::multiversion::java
