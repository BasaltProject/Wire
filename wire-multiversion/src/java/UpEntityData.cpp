#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/EntityDataIndexes.hpp>
#include <basalt/wire/multiversion/java/EntityDataTypes.hpp>
#include <basalt/wire/multiversion/java/UpEntityData.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

#include <algorithm>
#include <array>
#include <optional>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using Shape = play::ParticleOptionShape;

constexpr std::int32_t kUnknownType = -1;

// parity: Boat.Type of 1.21.1, by its id, as the 26.3 boat and chest boat types.
constexpr std::array<std::int32_t, 9> kBoatTypes = {90, 128, 12, 75, 0, 23, 34, 82, 9};
constexpr std::array<std::int32_t, 9> kChestBoatTypes = {91, 129, 13, 76, 1, 24, 35, 83, 8};
constexpr std::uint8_t kBoatVariantIndex = 11;
constexpr std::uint8_t kEffectParticlesIndex = 10;
constexpr std::int32_t kEntityEffectParticle = 28;
constexpr std::int32_t kOpaque = static_cast<std::int32_t>(0xFF000000U);
constexpr std::int32_t kCherryBoat = 5;
constexpr std::int32_t kOcelot = 92;
constexpr std::int32_t kCat = 21;
constexpr std::uint8_t kOcelotVariantIndex = 15;
constexpr std::uint8_t kCatVariantIndex = 20;
constexpr std::int32_t kFrog = 56;
constexpr std::uint8_t kFrogVariantIndex = 18;
// parity: the built-in CatVariant and FrogVariant ids before 1.21.5, as ids of the sorted registries the chain sends.
constexpr std::array<std::int32_t, 11> kCatVariants = {9, 1, 7, 8, 2, 3, 5, 6, 10, 4, 0};
constexpr std::array<std::int32_t, 3> kFrogVariants = {1, 2, 0};

[[nodiscard]] std::optional<std::int32_t> registryValue(const protocol::play::SetEntityDataPacket::Value& value) {
    if (const auto* number = std::get_if<protocol::play::SetEntityDataPacket::IntValue>(&value)) {
        return number->value;
    }
    if (const auto* id = std::get_if<protocol::play::SetEntityDataPacket::RegistryIdValue>(&value)) {
        return id->value;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> boatKind(std::int32_t type) {
    for (const auto* types : {&kBoatTypes, &kChestBoatTypes}) {
        if (std::ranges::find(*types, type) != types->end()) {
            return types == &kBoatTypes ? 0 : 1;
        }
    }
    return std::nullopt;
}

/// parity: the brewing stand's bottle and ingredient items, which servers before 26.3 never sent.
constexpr std::array<std::int32_t, 3> kBrewingInputs = {1442, 1271, 1445};
constexpr std::array<std::int32_t, 21> kBrewingReagents = {1001, 237,  1274, 1272, 831,  1441, 1403,
                                                           1269, 1279, 1,    1373, 1275, 824,  1064,
                                                           1383, 1193, 973,  1217, 1267, 1273, 1189};

void appendBrewingSets(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    const std::int32_t sets = protocol::readCollectionSize(input, 2);
    protocol::varInt::write(output, sets + 2);
    for (std::int32_t i = 0; i < sets && !input.failed(); ++i) {
        protocol::writeString(output, protocol::readString(input));
        const std::int32_t items = protocol::readCollectionSize(input);
        protocol::varInt::write(output, items);
        for (std::int32_t k = 0; k < items && !input.failed(); ++k) {
            protocol::varInt::write(output, protocol::varInt::read(input));
        }
    }
    if (input.failed()) {
        return;
    }
    const auto add = [&output](std::string_view key, std::span<const std::int32_t> items) {
        protocol::writeString(output, key);
        protocol::varInt::write(output, static_cast<std::int32_t>(items.size()));
        for (const std::int32_t item : items) {
            protocol::varInt::write(output, item);
        }
    };
    add("minecraft:brewing_input", kBrewingInputs);
    add("minecraft:brewing_reagent", kBrewingReagents);
    output.writeBytes(input.readable());
    packet.body = output.toVector();
}

struct RetypedField {
    std::int32_t entityType;
    std::int32_t index;
};

/// parity: 1.21.2 sent a salmon's size as a string; 1.21.4 sends its number.
constexpr std::int32_t kSalmonIn1_21_2 = 104;
constexpr std::int32_t kSalmonSizeIndex = 17;

constexpr std::array<RetypedField, 4> kRetypedIn1_21_11 = {{
    {152, 21}, // a wolf's anger end time went from remaining ticks to an absolute game time
    {11, 18},  // so did a bee's
    {159, 15}, // an avatar's main hand went from a byte to an enum
    {84, 15},
}};

constexpr std::int32_t kDrop = -1;
constexpr std::int32_t kSaddle = -2;
constexpr std::int32_t kHorseArmor = -3;
constexpr std::int32_t kCarpet = -4;
constexpr std::int32_t kCloudColor = -5;

constexpr std::array<std::int32_t, 7> kMinecarts = {25, 29, 57, 66, 86, 125, 137};
constexpr std::array<std::int32_t, 1> kPig = {101};
constexpr std::array<std::int32_t, 1> kStrider = {132};
constexpr std::array<std::int32_t, 1> kShulker = {115};
constexpr std::array<std::int32_t, 1> kVillager = {143};
constexpr std::array<std::int32_t, 3> kIllagers = {47, 69, 144};
constexpr std::array<std::int32_t, 3> kSkeletons = {118, 131, 150};
constexpr std::array<std::int32_t, 5> kZombies = {39, 68, 154, 157, 158};
constexpr std::array<std::int32_t, 3> kArrows = {6, 126, 139};
constexpr std::array<std::int32_t, 6> kHorses = {19, 37, 67, 88, 119, 155};
constexpr std::array<std::int32_t, 9> kOwnedHorses = {19, 20, 37, 67, 79, 88, 119, 138, 155};
constexpr std::array<std::int32_t, 1> kHorse = {67};
constexpr std::array<std::int32_t, 1> kWolf = {152};
constexpr std::array<std::int32_t, 1> kOcelots = {92};
constexpr std::array<std::int32_t, 2> kLlamas = {79, 138};
constexpr std::array<std::int32_t, 1> kClouds = {3};
constexpr std::array<std::int32_t, 1> kDolphin = {36};
constexpr std::array<std::int32_t, 1> kTurtle = {141};

/// A field a step dropped (`to` is kDrop, kSaddle or kHorseArmor) or moved, in the numbering of the older side of the
/// step.
struct ClimbRule {
    std::int32_t step;
    std::int32_t index;
    std::int32_t to;
    std::span<const std::int32_t> types;
};

constexpr std::array<ClimbRule, 22> kDroppedClimbing = {{
    {protocolVersions::kV1_13_2, 15, kDrop, kOcelots}, // parity: Ocelot's variant, owner and tame flags, gone in 1.14
    {protocolVersions::kV1_13_2, 14, kDrop, kOcelots},
    {protocolVersions::kV1_13_2, 13, kDrop, kOcelots},
    {protocolVersions::kV1_13_2, 16, kHorseArmor, kHorse}, // parity: Horse's armour, equipment since 1.14
    {protocolVersions::kV1_13_2, 12, kDrop, kIllagers},    // parity: AbstractIllager's flags, gone in 1.14
    {protocolVersions::kV1_13_2, 12, kDrop, kSkeletons},   // parity: AbstractSkeleton's swinging arms, gone in 1.14
    {protocolVersions::kV1_13_2, 14, kDrop, kZombies},     // parity: Zombie's raised arms, gone in 1.14
    {protocolVersions::kV1_13_2, 13, 15, kVillager},       // parity: the profession, which became VillagerData in 1.14
    {protocolVersions::kV1_15_2, 8, kDrop, kArrows},       // parity: AbstractArrow's owner, gone in 1.16
    {protocolVersions::kV1_14_4, 17, kDrop, kWolf},        // parity: Wolf's health, gone in 1.15
    {protocolVersions::kV1_16_4, 16, kDrop, kShulker},     // parity: Shulker's attached position, gone in 1.17
    {protocolVersions::kV1_19_3, 18, kDrop, kOwnedHorses}, // parity: AbstractHorse's owner, gone in 1.19.4
    {protocolVersions::kV1_20_3, 20, kCarpet, kLlamas},    // parity: Llama's carpet, equipment since 1.20.5
    {protocolVersions::kV1_20_3, 9, kCloudColor,
     kClouds}, // parity: AreaEffectCloud's colour, in its particle since 1.20.5
    {protocolVersions::kV1_21_4, 13, kDrop, kMinecarts},
    {protocolVersions::kV1_21_4, 17, kDrop, kDolphin}, // parity: Dolphin's treasure position, gone in 1.21.5
    {protocolVersions::kV1_21_4, 22, kDrop, kTurtle},  // parity: Turtle's travel and home fields, gone in 1.21.5
    {protocolVersions::kV1_21_4, 21, kDrop, kTurtle},
    {protocolVersions::kV1_21_4, 20, kDrop, kTurtle},
    {protocolVersions::kV1_21_4, 17, kDrop, kTurtle}, // parity: the custom display flag, gone in 1.21.5
    {protocolVersions::kV1_21_4, 17, kSaddle, kPig},
    {protocolVersions::kV1_21_4, 19, kSaddle, kStrider},
}};

[[nodiscard]] bool isAny(std::span<const std::int32_t> types, std::int32_t type) {
    return std::ranges::find(types, type) != types.end();
}

/// parity: VillagerDataFix of 1.14, for a profession without a career.
[[nodiscard]] std::int32_t professionOf(std::int32_t legacy) {
    constexpr std::array<std::int32_t, 6> kProfessions = {5, 9, 4, 1, 2, 11};
    return legacy >= 0 && legacy < 6 ? kProfessions[static_cast<std::size_t>(legacy)] : 0;
}

constexpr std::int32_t kPlainsVillager = 2;
constexpr std::int32_t kSaddleItem = 949;
// parity: HorseArmorType of 1.13 (iron, gold, diamond), as 26.3 items.
constexpr std::array<std::int32_t, 3> kHorseArmorItems = {1407, 1408, 1409};
// parity: DyeColor ids as the 26.3 carpets.
constexpr std::int32_t kWhiteCarpet = 580;
constexpr std::int32_t kDyeColors = 16;
constexpr std::uint8_t kHorseFlagsIndex = 18;
constexpr std::int8_t kHorseSaddled = 0x04;
constexpr std::uint8_t kMinecartBlockIndex = 11;
constexpr std::uint8_t kFireworkAttachedIndex = 9;
constexpr std::uint8_t kMooshroomVariantIndex = 18;
constexpr std::int32_t kMooshroom = 87;
constexpr std::int32_t kFirework = 54;
constexpr std::int32_t kZombieVillager = 157;
constexpr std::uint8_t kVillagerDataIndex = 19;
constexpr std::uint8_t kZombieVillagerDataIndex = 20;
constexpr std::int32_t kBlockStateSerializer = 14;
constexpr std::int32_t kOptionalBlockStateSerializer = 15;

} // namespace

UpEntityData::UpEntityData(PacketContentRewriter& next, const ComposedTables& tables, std::int32_t backendVersion)
    : next_(&next), tables_(&tables), backendVersion_(backendVersion) {
    const std::span<const std::int32_t> older = entityDataTypes(backendVersion);
    if (older.empty()) {
        return;
    }
    const std::int32_t highest = *std::ranges::max_element(older);
    serializers_.assign(static_cast<std::size_t>(highest + 1), -1);
    for (std::size_t kind = 0; kind < older.size(); ++kind) {
        if (older[kind] >= 0) {
            serializers_[static_cast<std::size_t>(older[kind])] = static_cast<std::int32_t>(kind);
        }
    }
    constexpr std::size_t kBlockStateKind = 14;
    constexpr std::size_t kOptionalBlockStateKind = 15;
    if (older.size() > kOptionalBlockStateKind) {
        olderBlockStateSerializer_ = older[kBlockStateKind];
        olderOptionalBlockStateSerializer_ = older[kOptionalBlockStateKind];
        for (const std::int32_t serializer : {olderBlockStateSerializer_, olderOptionalBlockStateSerializer_}) {
            if (serializer >= 0) {
                serializers_[static_cast<std::size_t>(serializer)] = -1;
            }
        }
        // parity: EntityDataSerializers.COMPOUND_TAG, the shoulder parrots until 1.21.9, after the optional state.
        if (backendVersion <= protocolVersions::kV1_21_7 && olderOptionalBlockStateSerializer_ >= 0) {
            olderCompoundSerializer_ = olderOptionalBlockStateSerializer_ + 1;
        }
    }
    for (const std::int32_t kind : rawKinds()) {
        if (static_cast<std::size_t>(kind) < older.size() && older[static_cast<std::size_t>(kind)] >= 0) {
            rawKinds_.push_back({older[static_cast<std::size_t>(kind)], kind});
            serializers_[static_cast<std::size_t>(older[static_cast<std::size_t>(kind)])] = -1;
        }
    }
    constexpr std::size_t kParticleKind = 16;
    if (backendVersion < protocolVersions::kV1_21_2 && older.size() > kParticleKind && older[kParticleKind] >= 0) {
        olderParticleSerializer_ = older[kParticleKind];
        serializers_[static_cast<std::size_t>(olderParticleSerializer_)] = -1;
    }
    constexpr std::size_t kItemKind = 7;
    if (backendVersion < protocolVersions::kV1_21_5 && older.size() > kItemKind && older[kItemKind] >= 0) {
        olderItemSerializer_ = older[kItemKind];
        serializers_[static_cast<std::size_t>(olderItemSerializer_)] = -1;
    }
    constexpr std::size_t kTextKind = 5;
    constexpr std::size_t kOptionalTextKind = 6;
    if (backendVersion < protocolVersions::kV1_20_3 && older.size() > kOptionalTextKind) {
        olderTextSerializer_ = older[kTextKind];
        olderOptionalTextSerializer_ = older[kOptionalTextKind];
        for (const std::int32_t serializer : {olderTextSerializer_, olderOptionalTextSerializer_}) {
            if (serializer >= 0) {
                serializers_[static_cast<std::size_t>(serializer)] = -1;
            }
        }
    }
}

bool UpEntityData::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    packet.becomes(packet.id);
    if (state == protocol::ProtocolState::Play) {
        if (packet.id == play::LoginPlayPacket::kId) {
            io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
            const std::int32_t entityId = input.readInt();
            if (!input.failed()) {
                types_[entityId] = play::SpawnEntityPacket::kPlayerEntityType;
            }
        } else if (packet.id == play::SpawnEntityPacket::kId) {
            io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
            const std::int32_t entityId = protocol::varInt::read(input);
            static_cast<void>(input.readSpan(16));
            const std::int32_t type = protocol::varInt::read(input);
            if (!input.failed()) {
                const std::int32_t mapped = tables_->entityType.map(type);
                types_[entityId] = mapped == IdMap::kNone ? kUnknownType : mapped;
            }
        } else if (packet.id == play::RemoveEntitiesPacket::kId) {
            io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
            if (const auto gone = play::RemoveEntitiesPacket::read(input)) {
                for (const std::int32_t entityId : gone->entityIds) {
                    types_.erase(entityId);
                    ocelots_.erase(entityId);
                }
            }
        } else if (packet.id == play::SetEntityDataPacket::kId && !entityData(packet)) {
            return false;
        }
    }
    const std::int32_t respawnId = respawn_;
    respawn_ = -1;
    if (!next_->clientbound(state, packet)) {
        equipment_.clear();
        return false;
    }
    for (const Worn& worn : equipment_) {
        io::ByteBuffer equipment;
        play::SetEquipmentPacket{
            worn.entityId,
            {{worn.slot,
              worn.item > 0 ? std::optional{play::WireItemStack{.itemId = worn.item, .count = 1}} : std::nullopt}}}
            .write(equipment);
        packet.extras.emplace_back(play::SetEquipmentPacket::kId, equipment.toVector());
    }
    equipment_.clear();
    if (state == protocol::ProtocolState::Play && backendVersion_ < protocolVersions::kV1_21_2) {
        trackRetyped(packet);
        if (respawnId >= 0) {
            respawn(packet, respawnId);
        }
    }
    if (state == protocol::ProtocolState::Play && packet.id == play::UpdateRecipesPacket::kId) {
        appendBrewingSets(packet);
    }
    return true;
}

bool UpEntityData::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    return next_->serverbound(state, packet);
}

std::int32_t UpEntityData::nativeIndex(std::int32_t nativeType, std::int32_t index) const {
    if (nativeType < 0) {
        return index;
    }
    // The links drop an index and close the gap; climbing opens the gap again, newest rule first.
    for (std::int32_t step = backendVersion_; step < protocolVersions::kNative; ++step) {
        const auto dropped = std::ranges::find_if(kDroppedClimbing, [&](const ClimbRule& rule) {
            return rule.step == step && rule.index <= index && isAny(rule.types, nativeType);
        });
        if (dropped != kDroppedClimbing.end()) {
            if (dropped->index == index && dropped->to < 0) {
                return dropped->to;
            }
            if (dropped->index == index) {
                index = dropped->to;
                continue;
            }
            index--;
        }
        const auto swapped = std::ranges::find_if(entityDataIndexSwaps(step), [&](const EntityDataIndexMove& move) {
            return move.to == index && isAny(move.types, nativeType);
        });
        if (swapped != entityDataIndexSwaps(step).end()) {
            index = swapped->from;
        }
        const std::span<const EntityDataIndexRule> rules = entityDataIndexRules(step);
        for (auto rule = rules.rbegin(); rule != rules.rend(); ++rule) {
            if (std::ranges::find(rule->types, nativeType) == rule->types.end()) {
                continue;
            }
            if (rule->shift && index >= rule->index) {
                index++;
            }
        }
        if (step == protocolVersions::kV1_21_9 &&
            std::ranges::any_of(kRetypedIn1_21_11, [&](const RetypedField& field) {
                return field.entityType == nativeType && field.index == index;
            })) {
            return -1;
        }
    }
    return index;
}

bool UpEntityData::entityData(MutablePacket& packet) {
    if (serializers_.empty()) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const auto shapeOf = [this](std::int32_t particleId) -> std::optional<Shape> {
        return nativeShape(tables_->particleType.map(particleId));
    };
    const play::SetEntityDataPacket::ForeignValue foreign =
        [this](std::int32_t serializer, io::ByteBuffer& in) -> std::optional<play::SetEntityDataPacket::Value> {
        if (serializer >= 0 && (serializer == olderTextSerializer_ || serializer == olderOptionalTextSerializer_)) {
            const bool optional = serializer == olderOptionalTextSerializer_;
            if (optional && !protocol::readBoolean(in)) {
                return play::SetEntityDataPacket::OptionalTextValue{std::nullopt};
            }
            const auto parsed = common::parseJson(protocol::readString(in));
            if (!parsed.has_value()) {
                return std::nullopt;
            }
            text::Component component = text::componentJson::deserialize(*parsed);
            if (optional) {
                return play::SetEntityDataPacket::OptionalTextValue{std::move(component)};
            }
            return play::SetEntityDataPacket::TextValue{std::move(component)};
        }
        if (serializer == olderItemSerializer_) {
            auto stack = olderItem(in);
            if (!stack) {
                return std::nullopt;
            }
            return play::SetEntityDataPacket::ItemValue{std::move(*stack)};
        }
        if (serializer >= 0 && serializer == olderParticleSerializer_) {
            return olderParticle(in);
        }
        if (const auto raw = std::ranges::find(rawKinds_, serializer, &RawKind::older); raw != rawKinds_.end()) {
            auto bytes = readRaw(raw->native, in);
            if (!bytes) {
                return std::nullopt;
            }
            return play::SetEntityDataPacket::RawValue{raw->native, std::move(*bytes)};
        }
        if (serializer >= 0 &&
            (serializer == olderBlockStateSerializer_ || serializer == olderOptionalBlockStateSerializer_)) {
            return blockState(serializer == olderBlockStateSerializer_ ? kBlockStateSerializer
                                                                       : kOptionalBlockStateSerializer,
                              protocol::varInt::read(in));
        }
        if (serializer < 0 || serializer != olderCompoundSerializer_) {
            return std::nullopt;
        }
        const nbt::NbtTag tag = protocol::readAnonymousNbt(in);
        const nbt::NbtCompound* shoulder = tag.get<nbt::NbtCompound>();
        std::optional<std::int32_t> variant;
        if (shoulder != nullptr && shoulder->contains("Variant")) {
            variant = shoulder->getInt("Variant", 0);
        }
        return play::SetEntityDataPacket::OptionalUnsignedIntValue{variant};
    };
    auto data = play::SetEntityDataPacket::read(input, shapeOf, serializers_, foreign);
    if (!data.has_value()) {
        return false;
    }
    const auto known = types_.find(data->entityId);
    std::int32_t type = known == types_.end() ? kUnknownType : known->second;
    if (backendVersion_ < protocolVersions::kV1_14 && type == kOcelot && becomesCat(*data)) {
        type = kCat;
    }
    const std::int32_t salmon =
        backendVersion_ < protocolVersions::kV1_21_4 ? tables_->entityType.map(kSalmonIn1_21_2) : IdMap::kNone;
    const bool boat = backendVersion_ < protocolVersions::kV1_21_2 && boatKind(type).has_value();
    std::optional<std::int32_t> variant;
    std::optional<std::int32_t> cloudColor;
    std::erase_if(data->entries, [&](play::SetEntityDataPacket::Entry& entry) {
        if (type == salmon && salmon != IdMap::kNone && entry.index == kSalmonSizeIndex) {
            if (const auto* size = std::get_if<play::SetEntityDataPacket::StringValue>(&entry.value)) {
                entry.value = play::SetEntityDataPacket::IntValue{size->value == "small"   ? 0
                                                                  : size->value == "large" ? 2
                                                                                           : 1};
            }
        }
        std::int32_t index = nativeIndex(type, entry.index);
        if (index == kSaddle) {
            if (const auto* saddled = std::get_if<play::SetEntityDataPacket::BoolValue>(&entry.value)) {
                equipment_.push_back(
                    {data->entityId, play::SetEquipmentPacket::SlotType::Saddle, saddled->value ? kSaddleItem : 0});
            }
            return true;
        }
        if (index == kCarpet) {
            if (const auto* dye = std::get_if<play::SetEntityDataPacket::IntValue>(&entry.value)) {
                const bool known = dye->value >= 0 && dye->value < kDyeColors;
                equipment_.push_back(
                    {data->entityId, play::SetEquipmentPacket::SlotType::Body, known ? kWhiteCarpet + dye->value : 0});
            }
            return true;
        }
        if (index == kCloudColor) {
            if (const auto* color = std::get_if<play::SetEntityDataPacket::IntValue>(&entry.value)) {
                cloudColor = color->value;
            }
            return true;
        }
        if (index == kHorseArmor) {
            if (const auto* armor = std::get_if<play::SetEntityDataPacket::IntValue>(&entry.value)) {
                const bool known =
                    armor->value > 0 && armor->value <= static_cast<std::int32_t>(kHorseArmorItems.size());
                equipment_.push_back({data->entityId, play::SetEquipmentPacket::SlotType::Body,
                                      known ? kHorseArmorItems[static_cast<std::size_t>(armor->value - 1)] : 0});
            }
            return true;
        }
        if (boat && index == kBoatVariantIndex) {
            if (const auto* wood = std::get_if<play::SetEntityDataPacket::IntValue>(&entry.value)) {
                // parity: Boat.Type of 1.19.4 put cherry at 5, after acacia.
                variant = backendVersion_ < protocolVersions::kV1_19_4 && wood->value >= kCherryBoat ? wood->value + 1
                                                                                                     : wood->value;
            }
            return true;
        }
        if (boat && index > kBoatVariantIndex) {
            index--;
        }
        if (index < 0) {
            return true;
        }
        entry.index = static_cast<std::uint8_t>(index);
        retype(data->entityId, type, entry);
        if (backendVersion_ < protocolVersions::kV1_21) {
            if (auto* id = std::get_if<play::SetEntityDataPacket::RegistryIdValue>(&entry.value);
                id != nullptr && id->registry == protocol::play::RegistryHolder::PaintingVariant) {
                id->value += 1;
            }
        }
        // The content rewriter cannot read these, so their particle ids are renumbered here.
        if (auto* particle = std::get_if<play::SetEntityDataPacket::ParticleValue>(&entry.value)) {
            if (cloudColor && particle->options &&
                std::holds_alternative<play::ParticleOptions::Color>(particle->options->payload())) {
                particle->options = play::ParticleOptions{play::ParticleOptions::Color{kOpaque | *cloudColor}};
            }
            particle->particleId = tables_->particleType.map(particle->particleId);
            return particle->particleId == IdMap::kNone;
        }
        if (auto* particles = std::get_if<play::SetEntityDataPacket::ParticlesValue>(&entry.value)) {
            for (play::SetEntityDataPacket::ParticleValue& each : particles->particles) {
                each.particleId = tables_->particleType.map(each.particleId);
            }
            return std::ranges::any_of(particles->particles, [](const play::SetEntityDataPacket::ParticleValue& each) {
                return each.particleId == IdMap::kNone;
            });
        }
        return false;
    });
    if (backendVersion_ < protocolVersions::kV1_20_5) {
        const std::span<const std::int32_t> living = entityDataIndexRules(protocolVersions::kV1_14_4).front().types;
        for (play::SetEntityDataPacket::Entry& entry : data->entries) {
            const auto* color = std::get_if<play::SetEntityDataPacket::IntValue>(&entry.value);
            if (entry.index != kEffectParticlesIndex || color == nullptr ||
                std::ranges::find(living, type) == living.end()) {
                continue;
            }
            // parity: LivingEntity.DATA_EFFECT_PARTICLES, which 1.20.5 made of the old potion colour.
            play::SetEntityDataPacket::ParticlesValue particles;
            if (color->value != 0) {
                particles.particles.push_back(
                    {kEntityEffectParticle,
                     play::ParticleOptions{play::ParticleOptions::Color{kOpaque | color->value}}});
            }
            entry.value = std::move(particles);
        }
    }
    if (const auto tracked = retyped_.find(data->entityId); variant && tracked != retyped_.end() &&
                                                            *variant != tracked->second.variant && *variant >= 0 &&
                                                            static_cast<std::size_t>(*variant) < kBoatTypes.size()) {
        tracked->second.variant = *variant;
        respawn_ = data->entityId;
    }
    io::ByteBuffer output;
    data->write(output);
    packet.body = output.toVector();
    return true;
}

bool UpEntityData::becomesCat(play::SetEntityDataPacket& data) {
    std::vector<play::SetEntityDataPacket::Entry>& sent = ocelots_[data.entityId];
    bool tamed = false;
    for (const play::SetEntityDataPacket::Entry& entry : data.entries) {
        if (const auto* variant = std::get_if<play::SetEntityDataPacket::IntValue>(&entry.value);
            variant != nullptr && entry.index == kOcelotVariantIndex && variant->value > 0) {
            tamed = true;
        }
        const auto same = std::ranges::find(sent, entry.index, &play::SetEntityDataPacket::Entry::index);
        if (same == sent.end()) {
            sent.push_back(entry);
        } else {
            *same = entry;
        }
    }
    if (!tamed) {
        return false;
    }
    // parity: 1.14 made a tamed ocelot a cat of the same colour; the cat comes back with everything the ocelot had.
    data.entries = std::move(sent);
    ocelots_.erase(data.entityId);
    types_[data.entityId] = kCat;
    respawn_ = data.entityId;
    return true;
}

void UpEntityData::retype(std::int32_t entityId, std::int32_t type, play::SetEntityDataPacket::Entry& entry) {
    using Data = play::SetEntityDataPacket;
    const auto* number = std::get_if<Data::IntValue>(&entry.value);
    if (backendVersion_ < protocolVersions::kV1_21_5) {
        const bool cat = type == kCat && entry.index == kCatVariantIndex;
        if (cat || (type == kFrog && entry.index == kFrogVariantIndex)) {
            const auto old = registryValue(entry.value);
            const std::span<const std::int32_t> ids = cat ? std::span<const std::int32_t>{kCatVariants} : kFrogVariants;
            const std::int32_t id = old && *old >= 0 && static_cast<std::size_t>(*old) < ids.size()
                                        ? ids[static_cast<std::size_t>(*old)]
                                        : ids.front();
            entry.value =
                Data::RegistryIdValue{id, cat ? play::RegistryHolder::CatVariant : play::RegistryHolder::FrogVariant};
            return;
        }
        if (number != nullptr && entry.index == kMinecartBlockIndex && isAny(kMinecarts, type)) {
            entry.value = blockState(kOptionalBlockStateSerializer, number->value);
            return;
        }
        if (const auto* flags = std::get_if<Data::ByteValue>(&entry.value);
            flags != nullptr && entry.index == kHorseFlagsIndex && isAny(kHorses, type)) {
            equipment_.push_back({entityId, play::SetEquipmentPacket::SlotType::Saddle,
                                  (flags->value & kHorseSaddled) != 0 ? kSaddleItem : 0});
        }
        if (const auto* variant = std::get_if<Data::StringValue>(&entry.value);
            variant != nullptr && type == kMooshroom && entry.index == kMooshroomVariantIndex) {
            // parity: MushroomCow.Variant, by id since 1.21.5.
            entry.value = Data::IntValue{variant->value == "brown" ? 1 : 0};
            return;
        }
    }
    if (backendVersion_ >= protocolVersions::kV1_14 || number == nullptr) {
        return;
    }
    if (type == kFirework && entry.index == kFireworkAttachedIndex) {
        // parity: FireworkRocketEntity.DATA_ATTACHED_TO_TARGET, an optional id since 1.14 and 0 for none before.
        entry.value = Data::OptionalUnsignedIntValue{number->value > 0 ? std::optional{number->value} : std::nullopt};
    } else if ((isAny(kVillager, type) && entry.index == kVillagerDataIndex) ||
               (type == kZombieVillager && entry.index == kZombieVillagerDataIndex)) {
        entry.value = Data::VillagerDataValue{kPlainsVillager, professionOf(number->value), 1};
    }
}

void UpEntityData::trackRetyped(const MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    if (packet.id == play::SpawnEntityPacket::kId) {
        const std::int32_t entityId = protocol::varInt::read(input);
        static_cast<void>(input.readSpan(16));
        const std::int32_t type = protocol::varInt::read(input);
        if (!input.failed() && (boatKind(type) || (backendVersion_ < protocolVersions::kV1_14 && type == kOcelot))) {
            retyped_[entityId] = Retyped{packet.body, {}, 0};
        } else {
            retyped_.erase(entityId);
        }
    } else if (packet.id == play::RemoveEntitiesPacket::kId) {
        if (const auto gone = play::RemoveEntitiesPacket::read(input)) {
            for (const std::int32_t entityId : gone->entityIds) {
                retyped_.erase(entityId);
            }
        }
    } else if (packet.id == play::SetPassengersPacket::kId) {
        const auto tracked = retyped_.find(protocol::varInt::read(input));
        if (tracked != retyped_.end() && !input.failed()) {
            tracked->second.passengers = packet.body;
        }
    }
}

void UpEntityData::respawn(MutablePacket& packet, std::int32_t entityId) {
    const auto tracked = retyped_.find(entityId);
    if (tracked == retyped_.end()) {
        return;
    }
    Retyped& entity = tracked->second;
    io::ByteBuffer input = io::ByteBuffer::wrap(entity.spawn);
    io::ByteBuffer spawn;
    protocol::varInt::write(spawn, protocol::varInt::read(input));
    spawn.writeBytes(input.readSpan(16));
    const std::int32_t type = protocol::varInt::read(input);
    const auto kind = boatKind(type);
    if (input.failed() || (!kind && type != kOcelot)) {
        return;
    }
    const auto& boats = kind == 0 ? kBoatTypes : kChestBoatTypes;
    protocol::varInt::write(spawn, kind ? boats[static_cast<std::size_t>(entity.variant)] : kCat);
    spawn.writeBytes(input.readable());
    entity.spawn = spawn.toVector();
    // A type cannot change in place: the entity leaves and comes back with its data and riders.
    io::ByteBuffer remove;
    play::RemoveEntitiesPacket{{entityId}}.write(remove);
    packet.extras.emplace_back(play::SpawnEntityPacket::kId, entity.spawn);
    packet.extras.emplace_back(play::SetEntityDataPacket::kId, std::move(packet.body));
    if (!entity.passengers.empty()) {
        packet.extras.emplace_back(play::SetPassengersPacket::kId, entity.passengers);
    }
    packet.id = play::RemoveEntitiesPacket::kId;
    packet.body = remove.toVector();
}

} // namespace basalt::wire::multiversion::java
