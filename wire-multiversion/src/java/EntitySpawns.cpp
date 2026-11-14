#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/EntitySpawns.hpp>
#include <basalt/wire/multiversion/java/NativeLiving.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using Spawn = MutablePacket::Spawn;

constexpr std::int32_t kExperienceOrb = 50;
constexpr std::int32_t kLightningBolt = 78;
constexpr std::int32_t kPainting = 94;

constexpr std::uint8_t kLastLivingIndex = 14;
constexpr std::uint8_t kLastMobIndex = 15;

constexpr std::array<std::int32_t, 4> kChestedHorses = {37, 79, 88, 138};
constexpr std::array<std::int32_t, 9> kHorses = {19, 20, 37, 67, 79, 88, 119, 138, 155};
constexpr std::array<std::int32_t, 5> kZombies = {39, 68, 154, 157, 158};
constexpr std::array<std::int32_t, 2> kGuardians = {41, 64};

/// parity: `AbstractMinecart.Type`, which before 1.14 rode in the object data of minecart 10.
constexpr std::array<std::pair<std::int32_t, std::int32_t>, 6> kMinecartKinds = {{
    {25, 1},
    {57, 2},
    {137, 3},
    {125, 4},
    {66, 5},
    {29, 6},
}};

struct Family {
    std::span<const std::int32_t> types;
    std::uint8_t last;
};

/// The classes below Mob a substitute can share with what it stands for, deepest first, and their last field.
constexpr std::array<Family, 4> kFamilies = {{
    {kChestedHorses, 19},
    {kHorses, 18},
    {kZombies, 18},
    {kGuardians, 17},
}};

/// The last field of the deepest of Entity, LivingEntity and Mob that a type extends.
[[nodiscard]] std::uint8_t lastBaseField(std::int32_t nativeType) {
    if (!nativeLiving(nativeType, protocolVersions::kNative)) {
        return play::SetEntityDataPacket::kLastSharedIndex;
    }
    if (nativeType == kArmorStand || nativeType == kMannequin) {
        return kLastLivingIndex;
    }
    return kLastMobIndex;
}

[[nodiscard]] std::uint8_t lastSharedField(std::int32_t nativeType, std::int32_t shown) {
    for (const Family& family : kFamilies) {
        if (std::ranges::contains(family.types, nativeType) && std::ranges::contains(family.types, shown)) {
            return family.last;
        }
    }
    return std::min(lastBaseField(nativeType), lastBaseField(shown));
}

} // namespace

bool EntitySpawns::living(std::int32_t nativeType) const noexcept { return nativeLiving(nativeType, farEnd_); }

const IdLookup& EntitySpawns::types(std::int32_t nativeType) const noexcept {
    if (farEnd_ < protocolVersions::kV1_14 && !living(nativeType)) {
        return tables_->objectType;
    }
    return tables_->entityType;
}

Spawn EntitySpawns::kindOf(std::int32_t nativeType) const noexcept {
    if (nativeType == kPainting && farEnd_ < protocolVersions::kV1_19) {
        return Spawn::kPainting;
    }
    if (nativeType == kExperienceOrb && farEnd_ < protocolVersions::kV1_21_5) {
        return Spawn::kExperienceOrb;
    }
    if (nativeType == kLightningBolt && farEnd_ < protocolVersions::kV1_16) {
        return Spawn::kLightning;
    }
    return living(nativeType) ? Spawn::kMob : Spawn::kObject;
}

bool EntitySpawns::spawn(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    auto spawned = play::SpawnEntityPacket::read(input);
    if (!spawned.has_value()) {
        return true;
    }
    const Spawn kind = kindOf(spawned->entityType);
    if (kind == Spawn::kPainting) {
        types_[spawned->entityId] = spawned->entityType;
        return false;
    }
    if (kind == Spawn::kExperienceOrb || kind == Spawn::kLightning) {
        types_[spawned->entityId] = spawned->entityType;
        packet.spawn = kind;
        return true;
    }
    const IdLookup& lookup = types(spawned->entityType);
    const std::int32_t type = lookup.map(spawned->entityType);
    if (type == IdLookup::kNone) {
        return spawned->entityType == tables_->nativePlayerEntityType();
    }
    types_[spawned->entityId] = spawned->entityType;
    packet.spawn = kind;
    if (lookup.substitutes(spawned->entityType)) {
        nameOver(packet, spawned->entityId, spawned->entityType);
    }
    if (farEnd_ < protocolVersions::kV1_14) {
        const auto minecart =
            std::ranges::find(kMinecartKinds, spawned->entityType, &std::pair<std::int32_t, std::int32_t>::first);
        if (minecart != kMinecartKinds.end()) {
            spawned->objectData = minecart->second;
        }
    }
    spawned->entityType = type;
    io::ByteBuffer output;
    spawned->write(output);
    packet.body = output.toVector();
    return true;
}

void EntitySpawns::forget(std::span<const std::int32_t> entityIds) {
    for (const std::int32_t entityId : entityIds) {
        types_.erase(entityId);
    }
}

std::int32_t EntitySpawns::typeOf(std::int32_t entityId) const {
    const auto found = types_.find(entityId);
    return found == types_.end() ? EntityDataRewriter::kUnknownType : found->second;
}

std::uint8_t EntitySpawns::lastKept(std::int32_t nativeType) const {
    if (nativeType < 0 || !types(nativeType).substitutes(nativeType)) {
        return kKeepAll;
    }
    const IdLookup& lookup = types(nativeType);
    const std::int32_t far = lookup.map(nativeType);
    const std::int32_t shown = far == IdLookup::kNone ? IdLookup::kNone : lookup.unmap(far);
    return shown == IdLookup::kNone ? play::SetEntityDataPacket::kLastSharedIndex : lastSharedField(nativeType, shown);
}

void EntitySpawns::nameOver(MutablePacket& packet, std::int32_t entityId, std::int32_t nativeType) const {
    if (labels_ == nullptr) {
        return;
    }
    std::optional<text::Component> label = labels_->entityLabel(nativeType);
    if (!label.has_value()) {
        return;
    }
    play::SetEntityDataPacket named;
    named.entityId = entityId;
    named.entries.push_back(
        {play::SetEntityDataPacket::kCustomNameIndex, play::SetEntityDataPacket::OptionalTextValue{*std::move(label)}});
    named.entries.push_back(
        {play::SetEntityDataPacket::kCustomNameVisibleIndex, play::SetEntityDataPacket::BoolValue{true}});
    std::vector<std::uint8_t> body;
    if (auto translated = entityData_->rewrite(named, nativeType)) {
        body = *std::move(translated);
    } else {
        io::ByteBuffer output;
        named.write(output);
        body = output.toVector();
    }
    packet.extras.emplace_back(play::SetEntityDataPacket::kId, std::move(body));
}

} // namespace basalt::wire::multiversion::java
