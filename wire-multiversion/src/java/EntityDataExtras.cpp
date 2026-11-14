// Native type ids and indexes are 26.3's; every other index is in the older numbering of its link.

#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/EntityDataExtras.hpp>
#include <basalt/wire/multiversion/java/NativeLiving.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::wire::multiversion::java {
namespace {

using Kind = EntityDataExtraKind;

constexpr std::int32_t kByte = 0;
constexpr std::int32_t kVarInt = 1;
constexpr std::int32_t kBoolean = 8;
constexpr std::int32_t kNitwitBefore1_14 = 5;
constexpr std::int32_t kHasChest = 0x08;
constexpr std::int32_t kMooshroomVariants = 0;
constexpr std::int32_t kSalmonSizes = 1;
constexpr std::array<std::string_view, 2> kMooshroomNames = {"red", "brown"};
constexpr std::array<std::string_view, 3> kSalmonSizeNames = {"small", "medium", "large"};

constexpr std::array<std::int32_t, 7> kMinecarts = {25, 29, 57, 66, 86, 125, 137};
constexpr std::array<std::int32_t, 2> kTurtleAndDolphin = {36, 141};
constexpr std::array<std::int32_t, 1> kPig = {101};
constexpr std::array<std::int32_t, 1> kMooshroom = {87};
constexpr std::array<std::int32_t, 1> kSalmon = {113};

constexpr std::array<std::int32_t, 20> kBoats = {0,  1,  8,  9,  12, 13, 23, 24, 34,  35,
                                                 75, 76, 82, 83, 90, 91, 95, 96, 128, 129};
constexpr std::array<std::int32_t, 4> kOakBoats = {90, 91, 95, 96};
constexpr std::array<std::int32_t, 2> kSpruceBoats = {128, 129};
constexpr std::array<std::int32_t, 2> kBirchBoats = {12, 13};
constexpr std::array<std::int32_t, 2> kJungleBoats = {75, 76};
constexpr std::array<std::int32_t, 2> kAcaciaBoats = {0, 1};
constexpr std::array<std::int32_t, 2> kCherryBoats = {23, 24};
constexpr std::array<std::int32_t, 2> kDarkOakBoats = {34, 35};
constexpr std::array<std::int32_t, 2> kMangroveBoats = {82, 83};
constexpr std::array<std::int32_t, 2> kBambooRafts = {8, 9};

constexpr std::array<std::int32_t, 9> kHorses = {19, 20, 37, 67, 79, 88, 119, 138, 155};
constexpr std::array<std::int32_t, 1> kAreaEffectCloud = {3};
constexpr std::array<std::int32_t, 1> kZombieVillager = {157};
constexpr std::array<std::int32_t, 1> kHusk = {68};
constexpr std::array<std::int32_t, 1> kWitherSkeleton = {150};
constexpr std::array<std::int32_t, 1> kStray = {131};
constexpr std::array<std::int32_t, 1> kHorse = {67};
constexpr std::array<std::int32_t, 3> kDonkeyAndLlamas = {37, 79, 138};
constexpr std::array<std::int32_t, 1> kMule = {88};
constexpr std::array<std::int32_t, 1> kZombieHorse = {155};
constexpr std::array<std::int32_t, 1> kSkeletonHorse = {119};
constexpr std::array<std::int32_t, 1> kGuardian = {64};
constexpr std::array<std::int32_t, 1> kElderGuardian = {41};
constexpr std::array<std::int32_t, 1> kVindicator = {144};
constexpr std::array<std::int32_t, 3> kArrows = {6, 126, 139};
constexpr std::array<std::int32_t, 1> kWolf = {152};
constexpr std::array<std::int32_t, 1> kShulker = {115};
constexpr std::array<std::int32_t, 1> kSkeleton = {118};

constexpr std::array<std::int32_t, 5> kZombies = {39, 68, 154, 157, 158};
constexpr std::array<std::int32_t, 5> kSkeletons = {16, 98, 118, 131, 150};
constexpr std::array<std::int32_t, 2> kLlamas = {79, 138};
constexpr std::array<std::int32_t, 4> kChestedHorses = {37, 79, 88, 138};
constexpr std::array<std::int32_t, 4> kIllagersAndVex = {47, 69, 142, 144};
constexpr std::array<std::int32_t, 1> kPolarBear = {105};

constexpr std::array<EntityDataIndexRule, 1> kDrops110 = {{
    {13, false, kPolarBear},
}};

constexpr std::array<EntityDataIndexRule, 7> kDrops210 = {{
    {13, false, kZombies},
    {15, false, kChestedHorses},
    {16, false, kLlamas},
    {17, false, kLlamas},
    {18, false, kLlamas},
    {12, false, kIllagersAndVex},
    {15, false, kShulker},
}};

constexpr std::array<EntityDataIndexMove, 4> kSwaps210 = {{
    {14, 15, kZombies},
    {15, 14, kZombies},
    {12, 13, kSkeletons},
    {14, 16, kHorses},
}};

/// parity: the kinds zombies, skeletons and horses told apart by a field before 1.11.
constexpr std::array<EntityDataExtra, 13> kExtras210 = {{
    {kZombieVillager, 13, Kind::kOffset, 20, 1},
    {kHusk, 13, Kind::kConstant, -1, 6, kVarInt, 210},
    {kWitherSkeleton, 12, Kind::kConstant, -1, 1, kVarInt},
    {kStray, 12, Kind::kConstant, -1, 2, kVarInt, 210},
    {kHorse, 14, Kind::kConstant, -1, 0, kVarInt},
    {kDonkeyAndLlamas, 14, Kind::kConstant, -1, 1, kVarInt},
    {kMule, 14, Kind::kConstant, -1, 2, kVarInt},
    {kZombieHorse, 14, Kind::kConstant, -1, 3, kVarInt},
    {kSkeletonHorse, 14, Kind::kConstant, -1, 4, kVarInt},
    {kGuardian, 12, Kind::kByteFromFlag, 16, 2, 0},
    {kElderGuardian, 12, Kind::kByteFromFlagAlways, 16, 2, 4},
    {kVindicator, 13, Kind::kConstant, -1, 4, kVarInt},
    {kChestedHorses, 13, Kind::kWithBit, 18, 19, kHasChest},
}};

constexpr std::array<EntityDataExtra, 1> kExtras393 = {{
    {kWolf, 17, Kind::kReverse, 21, 15},
}};

constexpr std::array<std::int32_t, 2> kMooshroomAndOcelot = {87, 92};
constexpr std::array<std::int32_t, 7> kThrownItems = {40, 45, 49, 51, 53, 121, 123};
constexpr std::array<std::int32_t, 1> kFirework = {54};

constexpr std::array<EntityDataIndexRule, 3> kDrops404 = {{
    {15, false, kMooshroomAndOcelot},
    {7, false, kThrownItems},
    {9, false, kFirework},
}};

constexpr std::array<EntityDataIndexMove, 3> kSwaps404 = {{
    {14, 15, kZombies},
    {15, 16, kZombies},
    {16, 17, kZombies},
}};

/// parity: Zombie and AbstractSkeleton raised their arms from a field of their own before 1.14.
constexpr std::array<EntityDataExtra, 2> kExtras404 = {{
    {kZombies, 14, Kind::kFlagFromBit, 15, 4},
    {kSkeletons, 12, Kind::kFlagFromBit, 15, 4},
}};

constexpr std::array<EntityDataIndexMove, 2> kSwaps498 = {{
    {17, 18, kWolf},
    {18, 19, kWolf},
}};

constexpr std::array<EntityDataExtra, 1> kExtras498 = {{
    {kWolf, 17, Kind::kCopy, 9},
}};

constexpr std::array<EntityDataIndexMove, 2> kSwaps578 = {{
    {8, 9, kArrows},
    {9, 10, kArrows},
}};

constexpr std::array<EntityDataIndexRule, 1> kDrops754 = {{
    {16, false, kSkeleton},
}};

constexpr std::array<EntityDataIndexMove, 2> kSwaps754 = {{
    {16, 17, kShulker},
    {17, 18, kShulker},
}};

constexpr std::array<EntityDataIndexMove, 4> kSwaps761 = {{
    {18, 19, kHorses},
    {19, 20, kHorses},
    {20, 21, kHorses},
    {21, 22, kHorses},
}};

constexpr std::array<EntityDataIndexMove, 3> kSwaps765 = {{
    {9, 10, kAreaEffectCloud},
    {10, 11, kAreaEffectCloud},
    {20, 21, kLlamas},
}};

constexpr std::array<EntityDataIndexMove, 3> kSwaps767 = {{
    {11, 12, kBoats},
    {12, 13, kBoats},
    {13, 14, kBoats},
}};

/// parity: Boat.Type before 1.21.2, the wood in declaration order.
constexpr std::array<EntityDataExtra, 9> kExtras767 = {{
    {kOakBoats, 11, Kind::kConstant, -1, 0, kVarInt},
    {kSpruceBoats, 11, Kind::kConstant, -1, 1, kVarInt},
    {kBirchBoats, 11, Kind::kConstant, -1, 2, kVarInt},
    {kJungleBoats, 11, Kind::kConstant, -1, 3, kVarInt},
    {kAcaciaBoats, 11, Kind::kConstant, -1, 4, kVarInt},
    {kCherryBoats, 11, Kind::kConstant, -1, 5, kVarInt},
    {kDarkOakBoats, 11, Kind::kConstant, -1, 6, kVarInt},
    {kMangroveBoats, 11, Kind::kConstant, -1, 7, kVarInt},
    {kBambooRafts, 11, Kind::kConstant, -1, 8, kVarInt},
}};

constexpr std::array<EntityDataIndexMove, 6> kSwaps769 = {{
    {17, 18, kTurtleAndDolphin},
    {18, 19, kTurtleAndDolphin},
    {17, 18, kPig},
    {11, 11, kMinecarts, kVarInt},
    {13, 14, kMinecarts},
    {14, 15, kMinecarts},
}};

constexpr std::array<EntityDataExtra, 2> kExtras769 = {{
    {kMinecarts, 13, Kind::kNonZero, 11},
    {kMooshroom, 17, Kind::kName, 18, kMooshroomVariants},
}};

constexpr std::int32_t kEffectParticles = 10;
constexpr std::int32_t kEntityEffectParticle = 28;
constexpr std::int32_t kRgb = 0xFFFFFF;
constexpr std::array<std::int32_t, 1> kPlayer = {protocol::play::SpawnEntityPacket::kPlayerEntityType};

/// parity: `LivingEntity.DATA_EFFECT_COLOR_ID`, one colour before 1.20.5 turned it into a list of particles.
constexpr std::array<EntityDataExtra, 2> kExtras765 = {{
    {kNativeLiving, 10, Kind::kEffectColour, kEffectParticles},
    {kPlayer, 10, Kind::kEffectColour, kEffectParticles},
}};

constexpr std::array<EntityDataExtra, 1> kExtras768 = {{
    {kSalmon, 17, Kind::kName, 17, kSalmonSizes},
}};

} // namespace

std::span<const EntityDataIndexRule> entityDataExtraDrops(std::int32_t link) {
    switch (link) {
    case 110:
        return kDrops110;
    case 210:
        return kDrops210;
    case 404:
        return kDrops404;
    case 754:
        return kDrops754;
    default:
        return {};
    }
}

std::span<const EntityDataIndexMove> entityDataExtraSwaps(std::int32_t link) {
    switch (link) {
    case 210:
        return kSwaps210;
    case 404:
        return kSwaps404;
    case 498:
        return kSwaps498;
    case 578:
        return kSwaps578;
    case 754:
        return kSwaps754;
    case 761:
        return kSwaps761;
    case 765:
        return kSwaps765;
    case 767:
        return kSwaps767;
    case 769:
        return kSwaps769;
    default:
        return {};
    }
}

std::span<const EntityDataExtra> entityDataExtras(std::int32_t link) {
    switch (link) {
    case 210:
        return kExtras210;
    case 393:
        return kExtras393;
    case 404:
        return kExtras404;
    case 498:
        return kExtras498;
    case 765:
        return kExtras765;
    case 767:
        return kExtras767;
    case 768:
        return kExtras768;
    case 769:
        return kExtras769;
    default:
        return {};
    }
}

std::int32_t legacyProfession(std::int32_t profession, std::int32_t farEnd) {
    switch (profession) {
    case 1:
    case 10:
    case 13:
    case 14:
        return 3;
    case 2:
    case 8:
        return 4;
    case 3:
    case 9:
        return 1;
    case 4:
        return 2;
    case 5:
    case 6:
    case 7:
    case 12:
        return 0;
    case 0:
    case 11:
        return farEnd < protocolVersions::kV1_11 ? 0 : kNitwitBefore1_14;
    default:
        // parity: before 1.11 there was no nitwit, and a profession past the last was read as a farmer.
        return farEnd < protocolVersions::kV1_11 ? 0 : kNitwitBefore1_14;
    }
}

std::optional<protocol::play::SetEntityDataPacket::Value>
extraValue(const EntityDataExtra& extra, const protocol::play::SetEntityDataPacket& data, std::int32_t farEnd) {
    using Packet = protocol::play::SetEntityDataPacket;
    const auto field = [&data](std::int32_t index) -> const Packet::Value* {
        const auto found = std::ranges::find(data.entries, index, &Packet::Entry::index);
        return found == data.entries.end() ? nullptr : &found->value;
    };
    const Packet::Value* source = field(extra.source);
    if (extra.kind == Kind::kConstant) {
        if (extra.b == kBoolean) {
            return Packet::BoolValue{extra.a != 0};
        }
        if (extra.b == kByte) {
            return Packet::ByteValue{static_cast<std::int8_t>(extra.a)};
        }
        return Packet::IntValue{extra.a};
    }
    if (source == nullptr) {
        return extra.kind == Kind::kByteFromFlagAlways
                   ? std::optional<Packet::Value>{Packet::ByteValue{static_cast<std::int8_t>(extra.b)}}
                   : std::nullopt;
    }
    if (extra.kind == Kind::kCopy) {
        return *source;
    }
    if (extra.kind == Kind::kEffectColour) {
        const auto* list = std::get_if<Packet::ParticlesValue>(source);
        if (list == nullptr) {
            return std::nullopt;
        }
        for (const Packet::ParticleValue& particle : list->particles) {
            if (particle.particleId != kEntityEffectParticle || !particle.options.has_value()) {
                continue;
            }
            if (const auto* colour = particle.options->as<protocol::play::ParticleOptions::Color>()) {
                return Packet::IntValue{colour->color & kRgb};
            }
        }
        return Packet::IntValue{0};
    }
    std::int32_t number = 0;
    if (const auto* flag = std::get_if<Packet::BoolValue>(source)) {
        number = flag->value ? 1 : 0;
    } else if (const auto* byte = std::get_if<Packet::ByteValue>(source)) {
        number = byte->value;
    } else if (const auto* integer = std::get_if<Packet::IntValue>(source)) {
        number = integer->value;
    } else if (const auto* registry = std::get_if<Packet::RegistryIdValue>(source)) {
        number = registry->value;
    } else if (const auto* villager = std::get_if<Packet::VillagerDataValue>(source)) {
        number = legacyProfession(villager->professionId, farEnd);
    } else if (const auto* raw = std::get_if<Packet::RawValue>(source)) {
        number = std::ranges::any_of(raw->bytes, [](std::uint8_t each) { return each != 0; }) ? 1 : 0;
    } else {
        return std::nullopt;
    }
    switch (extra.kind) {
    case Kind::kWithBit: {
        const auto* flag = std::get_if<Packet::BoolValue>(field(extra.a));
        return Packet::ByteValue{static_cast<std::int8_t>(flag != nullptr && flag->value ? number | extra.b : number)};
    }
    case Kind::kFlagFromBit:
        return Packet::BoolValue{(number & extra.a) != 0};
    case Kind::kByteFromFlag:
    case Kind::kByteFromFlagAlways:
        return Packet::ByteValue{static_cast<std::int8_t>((number != 0 ? extra.a : 0) | extra.b)};
    case Kind::kOffset:
        return Packet::IntValue{number + extra.a};
    case Kind::kReverse:
        return Packet::IntValue{extra.a - number};
    case Kind::kNonZero:
        return Packet::BoolValue{number != 0};
    case Kind::kName: {
        const std::span<const std::string_view> names = extra.a == kSalmonSizes
                                                            ? std::span<const std::string_view>{kSalmonSizeNames}
                                                            : std::span<const std::string_view>{kMooshroomNames};
        const auto at = static_cast<std::size_t>(std::clamp(number, 0, static_cast<std::int32_t>(names.size()) - 1));
        return Packet::StringValue{std::string{names[at]}};
    }
    default:
        return std::nullopt;
    }
}

bool replacesSource(EntityDataExtraKind kind) {
    switch (kind) {
    case Kind::kByteFromFlag:
    case Kind::kByteFromFlagAlways:
    case Kind::kOffset:
    case Kind::kReverse:
    case Kind::kName:
    case Kind::kEffectColour:
    case Kind::kWithBit:
        return true;
    default:
        return false;
    }
}

} // namespace basalt::wire::multiversion::java
