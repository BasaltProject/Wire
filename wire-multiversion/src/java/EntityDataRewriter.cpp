#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/EntityDataExtras.hpp>
#include <basalt/wire/multiversion/java/EntityDataIndexes.hpp>
#include <basalt/wire/multiversion/java/EntityDataRewriter.hpp>
#include <basalt/wire/multiversion/java/EntityDataTypes.hpp>
#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/multiversion/java/NativeParticles.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace basalt::wire::multiversion::java {
namespace {

using protocol::play::Pose;
using protocol::play::SetEntityDataPacket;

constexpr std::int32_t kNone = -1;
constexpr std::uint8_t kEndOfMetadata = 0xFF;
constexpr std::uint8_t kEndOfPackedMetadata = 0x7F;
constexpr int kPackedTypeShift = 5;
constexpr std::int32_t kPackedIndexMask = 0x1F;
constexpr std::int32_t kPackedTypeLimit = 7;
constexpr std::int32_t kPackedShort = 1;
constexpr std::int32_t kPackedInt = 2;

constexpr std::int32_t kByte = 0;
constexpr std::int32_t kVarInt = 1;
constexpr std::int32_t kLong = 2;
constexpr std::int32_t kFloat = 3;
constexpr std::int32_t kString = 4;
constexpr std::int32_t kComponent = 5;
constexpr std::int32_t kOptionalComponent = 6;
constexpr std::int32_t kItem = 7;
constexpr std::int32_t kBoolean = 8;
constexpr std::int32_t kRotations = 9;
constexpr std::int32_t kOptionalBlockPosition = 11;
constexpr std::int32_t kParticle = 16;
constexpr std::int32_t kParticles = 17;
constexpr std::int32_t kVillagerData = 18;
constexpr std::int32_t kOptionalUnsignedInt = 19;
constexpr std::int32_t kPose = 20;

/// parity: Player.DATA_SHOULDER_PARROT_LEFT and _RIGHT, a parrot variant since 1.21.9 and its whole tag before.
constexpr std::uint8_t kShoulderLeftIndex = 19;
constexpr std::uint8_t kShoulderRightIndex = 20;

constexpr std::int32_t kBlockPosition = 10;
constexpr std::int32_t kOptionalLivingReference = 13;
constexpr std::int32_t kBlockState = 14;
constexpr std::int32_t kOptionalBlockState = 15;
constexpr std::int32_t kOptionalGlobalPosition = 33;
constexpr std::int32_t kVector3 = 39;
constexpr std::int32_t kQuaternion = 40;
constexpr std::int32_t kResolvableProfile = 41;
constexpr std::size_t kUuidBytes = 16;
constexpr std::size_t kPositionBytes = 8;

/// The packet codec reads these itself; the rest are copied whole and placed by their serializer.
constexpr std::array<std::int32_t, kEntityDataTypeCount> kDecodedSerializers = [] {
    std::array<std::int32_t, kEntityDataTypeCount> decoded{};
    for (std::size_t serializer = 0; serializer < decoded.size(); ++serializer) {
        decoded[serializer] = static_cast<std::int32_t>(serializer);
    }
    for (const std::int32_t raw : {kBlockPosition, kOptionalLivingReference, kBlockState, kOptionalBlockState,
                                   kOptionalGlobalPosition, kVector3, kQuaternion, kResolvableProfile}) {
        decoded[static_cast<std::size_t>(raw)] = -1;
    }
    return decoded;
}();

void skipOptionalString(io::ByteBuffer& input) {
    if (protocol::readBoolean(input)) {
        static_cast<void>(protocol::readString(input));
    }
}

/// parity: ResolvableProfile.STREAM_CODEC, a whole or partial profile and then its skin patch.
void skipProfile(io::ByteBuffer& input) {
    if (protocol::readBoolean(input)) {
        input.skip(kUuidBytes);
        static_cast<void>(protocol::readString(input));
    } else {
        skipOptionalString(input);
        if (protocol::readBoolean(input)) {
            input.skip(kUuidBytes);
        }
    }
    const std::int32_t properties = protocol::readCollectionSize(input, 2);
    for (std::int32_t index = 0; index < properties && !input.failed(); ++index) {
        static_cast<void>(protocol::readString(input));
        static_cast<void>(protocol::readString(input));
        skipOptionalString(input);
    }
    for (int texture = 0; texture < 3; ++texture) {
        skipOptionalString(input);
    }
    if (protocol::readBoolean(input)) {
        static_cast<void>(protocol::readBoolean(input));
    }
}

[[nodiscard]] std::optional<SetEntityDataPacket::Value> readRaw(std::int32_t serializer, io::ByteBuffer& input) {
    const std::span<const std::uint8_t> before = input.readable();
    switch (serializer) {
    case kBlockPosition:
        input.skip(kPositionBytes);
        break;
    case kOptionalLivingReference:
        if (protocol::readBoolean(input)) {
            input.skip(kUuidBytes);
        }
        break;
    case kBlockState:
    case kOptionalBlockState:
        static_cast<void>(protocol::varInt::read(input));
        break;
    case kOptionalGlobalPosition:
        if (protocol::readBoolean(input)) {
            static_cast<void>(protocol::readString(input));
            input.skip(kPositionBytes);
        }
        break;
    case kVector3:
        input.skip(3 * sizeof(float));
        break;
    case kQuaternion:
        input.skip(4 * sizeof(float));
        break;
    case kResolvableProfile:
        skipProfile(input);
        break;
    default:
        return std::nullopt;
    }
    if (input.failed()) {
        return std::nullopt;
    }
    const std::span<const std::uint8_t> taken = before.first(before.size() - input.readable().size());
    return SetEntityDataPacket::RawValue{serializer, {taken.begin(), taken.end()}};
}

/// A copied value whose bytes the far end reads the same way, and which names nothing it numbers differently.
[[nodiscard]] bool rawFits(std::int32_t serializer, std::int32_t farEnd) {
    switch (serializer) {
    case kOptionalLivingReference:
    case kBlockState:
    case kOptionalBlockState:
    case kVector3:
    case kQuaternion:
        return true;
    case kBlockPosition:
        return farEnd >= protocolVersions::kV1_14;
    case kOptionalGlobalPosition:
        return farEnd >= protocolVersions::kV1_19;
    default:
        return farEnd == protocolVersions::kNative;
    }
}

constexpr std::int32_t kStanding = 0;
constexpr std::int32_t kSittingIn1_19_3 = 10;
constexpr std::int32_t kFirstPoseAddedIn1_19 = 8;
constexpr std::int32_t kFirstPoseAddedIn1_20_3 = 15;

[[nodiscard]] std::int32_t typeAt(std::span<const std::int32_t> types, std::int32_t index) {
    if (index < 0 || static_cast<std::size_t>(index) >= types.size()) {
        return kNone;
    }
    return types[static_cast<std::size_t>(index)];
}

[[nodiscard]] std::int32_t nativeTypeOf(const SetEntityDataPacket::Value& value) {
    return std::visit(
        [](const auto& held) -> std::int32_t {
            using T = std::decay_t<decltype(held)>;
            if constexpr (std::is_same_v<T, SetEntityDataPacket::ByteValue>) {
                return kByte;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::IntValue>) {
                return kVarInt;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::FloatValue>) {
                return kFloat;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::TextValue>) {
                return kComponent;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::OptionalTextValue>) {
                return kOptionalComponent;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::ItemValue>) {
                return kItem;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::BoolValue>) {
                return kBoolean;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::RotationsValue>) {
                return kRotations;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::OptionalBlockPosValue>) {
                return kOptionalBlockPosition;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::VillagerDataValue>) {
                return kVillagerData;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::PoseValue>) {
                return kPose;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::ParticleValue>) {
                return kParticle;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::ParticlesValue>) {
                return kParticles;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::OptionalUnsignedIntValue>) {
                return kOptionalUnsignedInt;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::LongValue>) {
                return kLong;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::StringValue>) {
                return kString;
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::RawValue>) {
                return held.serializer;
            } else {
                return static_cast<std::int32_t>(held.registry);
            }
        },
        value);
}

[[nodiscard]] std::int32_t compoundTagType(std::int32_t farEnd) {
    if (farEnd >= protocolVersions::kV1_19_4) {
        return 16;
    }
    if (farEnd >= protocolVersions::kV1_19_3) {
        return 15;
    }
    if (farEnd >= protocolVersions::kV1_13) {
        return 14;
    }
    return farEnd >= protocolVersions::kV1_12 ? 13 : kNone;
}

void writeShoulder(io::ByteBuffer& output, const SetEntityDataPacket::Value& value, std::int32_t index,
                   std::int32_t farEnd) {
    const auto* parrot = std::get_if<SetEntityDataPacket::OptionalUnsignedIntValue>(&value);
    const std::int32_t type = compoundTagType(farEnd);
    if (parrot == nullptr || type == kNone) {
        return;
    }
    nbt::NbtCompound tag;
    if (parrot->value.has_value()) {
        tag.putString("id", "minecraft:parrot");
        tag.putInt("Variant", *parrot->value);
    }
    output.writeByte(static_cast<std::uint8_t>(index));
    protocol::varInt::write(output, type);
    if (farEnd >= protocolVersions::kV1_20_2) {
        protocol::writeAnonymousNbt(output, nbt::NbtTag::ofCompound(std::move(tag)));
    } else {
        writeNamedNbt(output, nbt::NbtTag::ofCompound(std::move(tag)));
    }
}

void writeComponent(io::ByteBuffer& output, const text::Component& component, bool asLegacyName, std::int32_t farEnd) {
    if (asLegacyName) {
        protocol::writeString(output, text::toPlainText(component));
    } else if (farEnd >= protocolVersions::kV1_20_3) {
        protocol::writeAnonymousNbt(output, (farEnd >= protocolVersions::kV1_21_5
                                                 ? text::componentNbt::serialize(component)
                                                 : text::componentNbt::serializeLegacy(component)));
    } else {
        protocol::writeString(output, common::writeJson(text::componentJson::serializeLegacy(component)));
    }
}

} // namespace

EntityDataRewriter::EntityDataRewriter(std::int32_t farEnd, std::vector<std::int32_t> links, StackMapper stack,
                                       StateMapper blockState)
    : farEnd_(farEnd), links_(std::move(links)), stack_(std::move(stack)), blockState_(std::move(blockState)),
      types_(entityDataTypes(farEnd)), packed_(farEnd < protocolVersions::kV1_9) {}

std::optional<SetEntityDataPacket> EntityDataRewriter::read(io::ByteBuffer& input) {
    return SetEntityDataPacket::read(
        input, [](std::int32_t type) { return nativeParticles::shapeOf(type); }, kDecodedSerializers, readRaw);
}

std::optional<std::vector<std::uint8_t>>
EntityDataRewriter::rewrite(const SetEntityDataPacket& data, std::int32_t nativeType, std::uint8_t lastKept) const {
    if (types_.empty()) {
        return std::nullopt;
    }
    io::ByteBuffer output;
    protocol::varInt::write(output, data.entityId);
    for (const SetEntityDataPacket::Entry& entry : data.entries) {
        if (entry.index > lastKept || replaced(nativeType, entry.index)) {
            continue;
        }
        const Placement placement = placementFor(nativeType, entry.index, 0);
        if (placement.index < 0) {
            continue;
        }
        if (nativeType == protocol::play::SpawnEntityPacket::kPlayerEntityType &&
            farEnd_ < protocolVersions::kV1_21_9 &&
            (entry.index == kShoulderLeftIndex || entry.index == kShoulderRightIndex)) {
            writeShoulder(output, entry.value, placement.index, farEnd_);
        } else {
            writeEntry(output, entry, placement, types_);
        }
    }
    writeExtras(output, data, nativeType);
    output.writeByte(packed_ ? kEndOfPackedMetadata : kEndOfMetadata);
    return output.toVector();
}

bool EntityDataRewriter::replaced(std::int32_t nativeType, std::int32_t nativeIndex) const {
    for (const std::int32_t link : links_) {
        for (const EntityDataExtra& extra : entityDataExtras(link)) {
            if (extra.source == nativeIndex && farEnd_ >= extra.since && replacesSource(extra.kind) &&
                std::ranges::find(extra.types, nativeType) != extra.types.end()) {
                return true;
            }
        }
    }
    return false;
}

void EntityDataRewriter::writeExtras(io::ByteBuffer& output, const SetEntityDataPacket& data,
                                     std::int32_t nativeType) const {
    for (std::size_t step = 0; step < links_.size(); ++step) {
        for (const EntityDataExtra& extra : entityDataExtras(links_[step])) {
            if (farEnd_ < extra.since || std::ranges::find(extra.types, nativeType) == extra.types.end()) {
                continue;
            }
            const auto value = extraValue(extra, data, farEnd_);
            if (!value.has_value()) {
                continue;
            }
            const Placement placement = placementFor(nativeType, extra.index, step + 1);
            if (placement.index >= 0) {
                writeEntry(output, {static_cast<std::uint8_t>(placement.index), *value}, placement, types_);
            }
        }
    }
}

EntityDataRewriter::Placement EntityDataRewriter::placementFor(std::int32_t nativeType, std::int32_t nativeIndex,
                                                               std::size_t firstStep) const {
    std::int32_t index = nativeIndex;
    std::int32_t as = kNone;
    for (std::size_t step = firstStep; step < links_.size(); ++step) {
        const std::int32_t link = links_[step];
        for (const EntityDataIndexRule& drop : entityDataExtraDrops(link)) {
            if (drop.index == index && std::ranges::find(drop.types, nativeType) != drop.types.end()) {
                return {kNone, kNone};
            }
        }
        for (const EntityDataIndexRule& rule : entityDataIndexRules(link)) {
            if (nativeType < 0 || std::ranges::find(rule.types, nativeType) == rule.types.end()) {
                continue;
            }
            if (index == rule.index) {
                return {kNone, kNone};
            }
            if (rule.shift && index > rule.index) {
                index--;
            }
        }
        for (const std::span<const EntityDataIndexMove> swaps :
             {entityDataIndexSwaps(link), entityDataExtraSwaps(link)}) {
            const auto swap = std::ranges::find_if(swaps, [&](const EntityDataIndexMove& each) {
                return each.from == index && std::ranges::find(each.types, nativeType) != each.types.end();
            });
            if (swap != swaps.end()) {
                index = swap->to;
                as = swap->as < 0 ? as : swap->as;
                break;
            }
        }
        const std::span<const EntityDataIndexMove> moves = entityDataIndexMoves(link);
        if (moves.empty()) {
            continue;
        }
        if (nativeType < 0) {
            return {kNone, kNone};
        }
        const auto found = std::ranges::find_if(moves, [&](const EntityDataIndexMove& move) {
            return move.from == index && std::ranges::find(move.types, nativeType) != move.types.end();
        });
        if (found == moves.end()) {
            return {kNone, kNone};
        }
        index = found->to;
        as = found->as < 0 ? as : found->as;
    }
    return {index, as};
}

void EntityDataRewriter::writeEntry(io::ByteBuffer& output, const SetEntityDataPacket::Entry& source,
                                    Placement placement, std::span<const std::int32_t> types) const {
    SetEntityDataPacket::Value value = source.value;
    // The same field as the far end means it, or nothing for one it cannot be told at all.
    if (const auto* pose = std::get_if<SetEntityDataPacket::PoseValue>(&value)) {
        auto ordinal = static_cast<std::int32_t>(pose->pose);
        if (farEnd_ < protocolVersions::kV1_20_3 && ordinal >= kFirstPoseAddedIn1_20_3) {
            return;
        }
        if (farEnd_ < protocolVersions::kV1_19_3) {
            if (ordinal == kSittingIn1_19_3) {
                ordinal = kStanding;
            } else if (ordinal > kSittingIn1_19_3) {
                ordinal--;
            }
        }
        if (farEnd_ < protocolVersions::kV1_19 && ordinal >= kFirstPoseAddedIn1_19) {
            ordinal = kStanding;
        }
        value = SetEntityDataPacket::PoseValue{static_cast<Pose>(ordinal)};
    } else if (const auto* villager = std::get_if<SetEntityDataPacket::VillagerDataValue>(&value)) {
        if (farEnd_ < protocolVersions::kV1_14) {
            value = SetEntityDataPacket::IntValue{legacyProfession(villager->professionId, farEnd_)};
        }
    }
    if (placement.as >= 0) {
        writeAs(output, value, placement);
        return;
    }
    const std::int32_t index = placement.index;

    std::int32_t mapped = typeAt(types, nativeTypeOf(value));
    const bool asLegacyName = mapped == kNone && std::holds_alternative<SetEntityDataPacket::OptionalTextValue>(value);
    if (asLegacyName) {
        mapped = typeAt(types, kString);
    }
    const bool asByte = mapped == kNone && std::holds_alternative<SetEntityDataPacket::BoolValue>(value);
    if (asByte) {
        mapped = typeAt(types, kByte);
    }
    const auto* registryValue = std::get_if<SetEntityDataPacket::RegistryIdValue>(&value);
    const bool armAsByte = mapped == kNone && registryValue != nullptr &&
                           registryValue->registry == protocol::play::RegistryHolder::HumanoidArm;
    if (armAsByte) {
        mapped = typeAt(types, kByte);
    }
    if (mapped == kNone) {
        return;
    }

    io::ByteBuffer payload;
    bool written = true;
    std::visit(
        [&](const auto& held) {
            using T = std::decay_t<decltype(held)>;
            if constexpr (std::is_same_v<T, SetEntityDataPacket::ByteValue>) {
                payload.writeByte(static_cast<std::uint8_t>(held.value));
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::IntValue>) {
                protocol::varInt::write(payload, held.value);
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::FloatValue>) {
                payload.writeFloat(held.value);
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::BoolValue>) {
                if (asByte) {
                    payload.writeByte(held.value ? 1 : 0);
                } else {
                    protocol::writeBoolean(payload, held.value);
                }
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::RotationsValue>) {
                payload.writeFloat(held.x);
                payload.writeFloat(held.y);
                payload.writeFloat(held.z);
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::TextValue>) {
                writeComponent(payload, held.value, asLegacyName, farEnd_);
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::OptionalTextValue>) {
                if (asLegacyName) {
                    protocol::writeString(payload, held.value.has_value() ? text::toPlainText(*held.value) : "");
                } else {
                    protocol::writeBoolean(payload, held.value.has_value());
                    if (held.value.has_value()) {
                        writeComponent(payload, *held.value, false, farEnd_);
                    }
                }
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::PoseValue>) {
                protocol::varInt::write(payload, static_cast<std::int32_t>(held.pose));
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::VillagerDataValue>) {
                protocol::varInt::write(payload, held.typeId);
                protocol::varInt::write(payload, held.professionId);
                protocol::varInt::write(payload, held.level);
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::RegistryIdValue>) {
                if (armAsByte) {
                    payload.writeByte(static_cast<std::uint8_t>(held.value));
                } else {
                    protocol::varInt::write(payload, held.value);
                }
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::ParticleValue>) {
                if (farEnd_ != protocolVersions::kNative) {
                    written = false;
                    return;
                }
                protocol::varInt::write(payload, held.particleId);
                if (held.options) {
                    held.options->write(payload);
                }
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::ParticlesValue>) {
                // The same renumbering; an older client simply draws no effect swirl.
                if (farEnd_ != protocolVersions::kNative) {
                    written = false;
                    return;
                }
                protocol::varInt::write(payload, static_cast<std::int32_t>(held.particles.size()));
                for (const SetEntityDataPacket::ParticleValue& each : held.particles) {
                    protocol::varInt::write(payload, each.particleId);
                    if (each.options) {
                        each.options->write(payload);
                    }
                }
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::OptionalUnsignedIntValue>) {
                protocol::varInt::write(payload, held.value.has_value() ? *held.value + 1 : 0);
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::LongValue>) {
                if (farEnd_ != protocolVersions::kNative) {
                    written = false;
                    return;
                }
                protocol::varLong::write(payload, held.value);
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::StringValue>) {
                protocol::writeString(payload, held.value);
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::ItemValue>) {
                legacyStacks::write(payload, stack_(held.stack), farEnd_);
            } else if constexpr (std::is_same_v<T, SetEntityDataPacket::RawValue>) {
                written = rawFits(held.serializer, farEnd_);
                if (written && (held.serializer == kBlockState || held.serializer == kOptionalBlockState)) {
                    io::ByteBuffer state = io::ByteBuffer::wrap(held.bytes);
                    protocol::varInt::write(payload, entityState(protocol::varInt::read(state)));
                } else if (written) {
                    payload.writeBytes(held.bytes);
                }
            } else {
                // The packing of a position changed in 1.14, and this only ever carries a bed.
                if (farEnd_ < protocolVersions::kV1_14) {
                    written = false;
                    return;
                }
                protocol::writeBoolean(payload, held.pos.has_value());
                if (held.pos.has_value()) {
                    payload.writeLong(held.pos->pack());
                }
            }
        },
        value);
    if (written) {
        writeHeader(output, index, mapped, payload.readable());
    }
}

void EntityDataRewriter::writeAs(io::ByteBuffer& output, const SetEntityDataPacket::Value& value,
                                 Placement placement) const {
    std::optional<std::int32_t> number;
    if (const auto* byte = std::get_if<SetEntityDataPacket::ByteValue>(&value)) {
        number = byte->value;
    } else if (const auto* integer = std::get_if<SetEntityDataPacket::IntValue>(&value)) {
        number = integer->value;
    } else if (const auto* flag = std::get_if<SetEntityDataPacket::BoolValue>(&value)) {
        number = !flag->value ? 0 : (placement.as == kEntityDataChildByte ? -1 : 1);
    } else if (const auto* registry = std::get_if<SetEntityDataPacket::RegistryIdValue>(&value)) {
        number = registry->value;
    } else if (const auto* raw = std::get_if<SetEntityDataPacket::RawValue>(&value);
               raw != nullptr && (raw->serializer == kBlockState || raw->serializer == kOptionalBlockState)) {
        io::ByteBuffer state = io::ByteBuffer::wrap(raw->bytes);
        number = entityState(protocol::varInt::read(state));
    }
    if (!number.has_value()) {
        return;
    }
    const std::int32_t type = placement.as == kEntityDataChildByte ? kByte : placement.as;
    io::ByteBuffer payload;
    if (type == kByte) {
        payload.writeByte(static_cast<std::uint8_t>(*number));
    } else if (packed_ && type == kPackedShort) {
        payload.writeShort(static_cast<std::int16_t>(*number));
    } else if (packed_ && type == kPackedInt) {
        payload.writeInt(*number);
    } else if (!packed_ && type == kVarInt) {
        protocol::varInt::write(payload, *number);
    } else {
        return;
    }
    writeHeader(output, placement.index, type, payload.readable());
}

std::int32_t EntityDataRewriter::entityState(std::int32_t native) const {
    const std::int32_t mapped = native == 0 || !blockState_ ? native : blockState_(native);
    if (farEnd_ >= protocolVersions::kV1_13) {
        return mapped;
    }
    // parity: Block.getStateId before 1.13, the block in the low twelve bits and its data above them.
    return (mapped >> 4) | ((mapped & 0xF) << 12);
}

void EntityDataRewriter::writeHeader(io::ByteBuffer& output, std::int32_t index, std::int32_t type,
                                     std::span<const std::uint8_t> payload) const {
    if (packed_) {
        if (index > kPackedIndexMask || type > kPackedTypeLimit) {
            return;
        }
        output.writeByte(static_cast<std::uint8_t>((type << kPackedTypeShift) | index));
    } else {
        output.writeByte(static_cast<std::uint8_t>(index));
        protocol::varInt::write(output, type);
    }
    output.writeBytes(payload);
}

} // namespace basalt::wire::multiversion::java
