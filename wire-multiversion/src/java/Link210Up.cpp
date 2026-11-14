#include <basalt/wire/multiversion/java/Content340.hpp>
#include <basalt/wire/multiversion/java/Flattening340.hpp>
#include <basalt/wire/multiversion/java/Link210Up.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
namespace flat = flattening340;

constexpr std::int32_t kSpawnMob = 0x03;
constexpr std::int32_t kBlockEntity = 0x09;
constexpr std::int32_t kWindowItems = 0x14;
constexpr std::int32_t kSetSlot = 0x16;
constexpr std::int32_t kChunk = 0x20;
constexpr std::int32_t kDestroyEntities = 0x30;
constexpr std::int32_t kEntityData = 0x39;
constexpr std::int32_t kTitle = 0x45;
constexpr std::int32_t kSound = 0x46;
constexpr std::int32_t kCollect = 0x48;

constexpr std::int32_t kChat = 0x02;
constexpr std::int32_t kCreativeSlot = 0x18;
constexpr std::int32_t kUseItemOn = 0x1C;

constexpr std::int32_t kElderGuardian = 4;
constexpr std::int32_t kWitherSkeleton = 5;
constexpr std::int32_t kStray = 6;
constexpr std::int32_t kHusk = 23;
constexpr std::int32_t kZombieVillager = 27;
constexpr std::int32_t kSkeletonHorse = 28;
constexpr std::int32_t kZombieHorse = 29;
constexpr std::int32_t kDonkey = 31;
constexpr std::int32_t kMule = 32;
constexpr std::int32_t kSkeleton = 51;
constexpr std::int32_t kZombie = 54;
constexpr std::int32_t kGuardian = 68;
constexpr std::int32_t kHorse = 100;

constexpr std::int32_t kTypeByte = 0;
constexpr std::int32_t kTypeVarInt = 1;
constexpr std::int32_t kTypeBoolean = 6;
constexpr std::uint8_t kEnd = 0xFF;
constexpr std::int32_t kSpawnEgg = 383;
constexpr std::int16_t kEmpty = -1;
constexpr std::size_t kChatLength = 100;
constexpr float kCursorScale = 16.0F;

struct Entry {
    std::uint8_t index;
    std::int32_t type;
    std::vector<std::uint8_t> value;
};

[[nodiscard]] std::string_view renamed(std::span<const flat::Rename> names, std::string_view name, bool up) {
    for (const flat::Rename& one : names) {
        if ((up ? one.from : one.to) == name) {
            return up ? one.to : one.from;
        }
    }
    return name;
}

[[nodiscard]] bool readEntries(io::ByteBuffer& in, std::vector<Entry>& entries) {
    while (!in.failed()) {
        const std::uint8_t index = in.readByte();
        if (index == kEnd) {
            return true;
        }
        const std::int32_t type = in.readByte();
        io::ByteBuffer value;
        if (!content340::copyEntityValue(in, value, type)) {
            return false;
        }
        entries.push_back(Entry{index, type, value.toVector()});
    }
    return false;
}

[[nodiscard]] std::int32_t numberOf(const std::vector<Entry>& entries, std::uint8_t index) {
    for (const Entry& entry : entries) {
        if (entry.index != index || entry.value.empty()) {
            continue;
        }
        if (entry.type == kTypeByte) {
            return entry.value[0];
        }
        if (entry.type == kTypeVarInt) {
            io::ByteBuffer in = io::ByteBuffer::wrap(entry.value);
            return pv::varInt::read(in);
        }
    }
    return -1;
}

/// The 1.11 type of a mob 1.10 told apart by its data.
[[nodiscard]] std::int32_t splitType(std::int32_t type, std::vector<Entry>& entries) {
    switch (type) {
    case kGuardian:
        return (numberOf(entries, 12) & 0x04) != 0 && numberOf(entries, 12) >= 0 ? kElderGuardian : type;
    case kSkeleton: {
        const std::int32_t kind = numberOf(entries, 12);
        return kind == 1 ? kWitherSkeleton : kind == 2 ? kStray : type;
    }
    case kZombie: {
        const std::int32_t kind = numberOf(entries, 13);
        if (kind > 0 && kind < 6) {
            io::ByteBuffer profession;
            pv::varInt::write(profession, kind - 1);
            entries.push_back(Entry{16, kTypeVarInt, profession.toVector()});
            return kZombieVillager;
        }
        return kind == 6 ? kHusk : type;
    }
    case kHorse: {
        constexpr std::array<std::int32_t, 5> kHorses = {kHorse, kDonkey, kMule, kZombieHorse, kSkeletonHorse};
        const std::int32_t kind = numberOf(entries, 14);
        return kind >= 0 && kind < 5 ? kHorses[static_cast<std::size_t>(kind)] : type;
    }
    default:
        return type;
    }
}

void writeEntries(io::ByteBuffer& out, const std::vector<Entry>& entries, std::int32_t type) {
    const bool guardian = type == kGuardian || type == kElderGuardian;
    const bool skeleton = type == kSkeleton || type == kWitherSkeleton || type == kStray;
    const bool zombie = type == kZombie || type == kHusk || type == kZombieVillager;
    const bool horse =
        type == kHorse || type == kDonkey || type == kMule || type == kZombieHorse || type == kSkeletonHorse;
    for (const Entry& entry : entries) {
        std::int32_t index = entry.index;
        if (guardian && index == 12 && entry.type == kTypeByte) {
            out.writeByte(12);
            out.writeByte(kTypeBoolean);
            out.writeByte((entry.value[0] & 0x02) != 0 ? 1 : 0);
            continue;
        }
        if (skeleton && index == 12) {
            continue;
        }
        if (zombie && index == 14) {
            if (type != kZombieVillager) {
                continue;
            }
            index = 15;
        } else if (zombie && index == 15) {
            index = 14;
        }
        if (horse) {
            if (index == 14) {
                continue;
            }
            index = index == 16 ? 14 : index == 17 ? 16 : index;
            if (type != kHorse && (index == 15 || index == 16)) {
                continue;
            }
            if ((type == kDonkey || type == kMule) && index == 13 && entry.type == kTypeByte) {
                out.writeByte(15);
                out.writeByte(kTypeBoolean);
                out.writeByte((entry.value[0] & 0x08) != 0 ? 1 : 0);
            }
        }
        out.writeByte(static_cast<std::uint8_t>(index));
        out.writeByte(static_cast<std::uint8_t>(entry.type));
        out.writeBytes(entry.value);
    }
    out.writeByte(kEnd);
}

/// Copies a stack, with the spawn egg's entity named as the other side names it.
void copyStack(io::ByteBuffer& in, io::ByteBuffer& out, bool up) {
    const std::int16_t id = in.readShort();
    out.writeShort(id);
    if (id == kEmpty) {
        return;
    }
    out.writeBytes(in.readSpan(3));
    const auto named = readNamedNbt(in);
    if (!named) {
        return;
    }
    const nbt::NbtCompound* tag = named->tag.get<nbt::NbtCompound>();
    const nbt::NbtCompound* entity = tag != nullptr ? tag->getCompound("EntityTag") : nullptr;
    if (id != kSpawnEgg || entity == nullptr || !entity->contains("id")) {
        out.writeBytes(named->raw);
        return;
    }
    nbt::NbtCompound changedEntity = *entity;
    changedEntity.putString("id", std::string{renamed(flat::entityNames(), entity->getString("id"), up)});
    nbt::NbtCompound changed = *tag;
    changed.set("EntityTag", nbt::NbtTag::ofCompound(std::move(changedEntity)));
    writeNamedNbt(out, nbt::NbtTag::ofCompound(std::move(changed)));
}

/// A block entity with its 1.11 id, and a spawner's entity with its 1.11 name.
void writeBlockEntity(io::ByteBuffer& out, const NamedNbt& entity) {
    const nbt::NbtCompound* tag = entity.tag.get<nbt::NbtCompound>();
    if (tag == nullptr) {
        out.writeBytes(entity.raw);
        return;
    }
    nbt::NbtCompound changed = *tag;
    changed.putString("id", std::string{renamed(flat::blockEntityNames(), tag->getString("id"), true)});
    if (const nbt::NbtCompound* spawn = tag->getCompound("SpawnData")) {
        nbt::NbtCompound data = *spawn;
        data.putString("id", std::string{renamed(flat::entityNames(), spawn->getString("id"), true)});
        changed.set("SpawnData", nbt::NbtTag::ofCompound(std::move(data)));
    }
    if (tag->contains("EntityId")) {
        changed.putString("EntityId", std::string{renamed(flat::entityNames(), tag->getString("EntityId"), true)});
    }
    writeNamedNbt(out, nbt::NbtTag::ofCompound(std::move(changed)));
}

} // namespace

bool Link210Up::entities(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    std::vector<Entry> entries;
    if (packet.id == kSpawnMob) {
        const std::int32_t entity = pv::varInt::read(in);
        pv::varInt::write(out, entity);
        out.writeBytes(in.readSpan(16));
        const std::int32_t legacy = in.readByte();
        const std::span<const std::uint8_t> motion = in.readSpan(24 + 3 + 6);
        if (!readEntries(in, entries)) {
            return false;
        }
        const std::int32_t type = splitType(legacy, entries);
        types_[entity] = type;
        pv::varInt::write(out, type);
        out.writeBytes(motion);
        writeEntries(out, entries, type);
    } else {
        const std::int32_t entity = pv::varInt::read(in);
        pv::varInt::write(out, entity);
        if (!readEntries(in, entries)) {
            return false;
        }
        const auto type = types_.find(entity);
        writeEntries(out, entries, type == types_.end() ? -1 : type->second);
    }
    if (in.failed()) {
        return false;
    }
    packet.body = out.toVector();
    return true;
}

bool Link210Up::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kSpawnMob:
    case kEntityData:
        return entities(packet);
    case kDestroyEntities: {
        const std::int32_t count = pv::readCollectionSize(in);
        for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
            types_.erase(pv::varInt::read(in));
        }
        return true;
    }
    case kSound: {
        const std::int32_t sound = flat::sounds210().map(pv::varInt::read(in));
        if (sound == IdMap::kNone) {
            return false;
        }
        pv::varInt::write(out, sound);
        break;
    }
    case kTitle: {
        const std::int32_t action = pv::varInt::read(in);
        pv::varInt::write(out, action >= 2 ? action + 1 : action);
        break;
    }
    case kCollect:
        out.writeBytes(in.readable());
        pv::varInt::write(out, 1);
        packet.body = out.toVector();
        return true;
    case kWindowItems: {
        out.writeByte(in.readByte());
        const std::int16_t count = in.readShort();
        out.writeShort(count);
        for (std::int16_t slot = 0; slot < count && !in.failed(); ++slot) {
            copyStack(in, out, true);
        }
        break;
    }
    case kSetSlot:
        out.writeByte(in.readByte());
        out.writeShort(in.readShort());
        copyStack(in, out, true);
        break;
    case kBlockEntity: {
        out.writeLong(in.readLong());
        out.writeByte(in.readByte());
        const auto entity = readNamedNbt(in);
        if (!entity) {
            return false;
        }
        writeBlockEntity(out, *entity);
        break;
    }
    case kChunk: {
        out.writeBytes(in.readSpan(4 + 4 + 1));
        pv::varInt::write(out, pv::varInt::read(in));
        const std::int32_t length = pv::varInt::read(in);
        pv::varInt::write(out, length);
        out.writeBytes(in.readSpan(static_cast<std::size_t>(std::max(length, 0))));
        const std::int32_t count = pv::readCollectionSize(in);
        pv::varInt::write(out, count);
        for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
            const auto entity = readNamedNbt(in);
            if (!entity) {
                return false;
            }
            writeBlockEntity(out, *entity);
        }
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

bool Link210Up::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    switch (packet.id) {
    case kChat: {
        std::string message = pv::readString(in);
        if (message.size() > kChatLength) {
            message.resize(kChatLength);
        }
        pv::writeString(out, message);
        break;
    }
    case kUseItemOn:
        out.writeLong(in.readLong());
        pv::varInt::write(out, pv::varInt::read(in));
        pv::varInt::write(out, pv::varInt::read(in));
        for (int axis = 0; axis < 3; ++axis) {
            out.writeByte(static_cast<std::uint8_t>(std::clamp(in.readFloat() * kCursorScale, 0.0F, 255.0F)));
        }
        break;
    case kCreativeSlot:
        out.writeShort(in.readShort());
        copyStack(in, out, false);
        break;
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
