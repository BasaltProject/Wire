#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/EntityDataIndexes.hpp>
#include <basalt/wire/multiversion/java/EntityDataRewriter.hpp>
#include <basalt/wire/multiversion/java/EntityDataTypes.hpp>
#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
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

constexpr std::int32_t kStanding = 0;
constexpr std::int32_t kSittingIn1_19_3 = 10;
constexpr std::int32_t kFirstPoseAddedIn1_19 = 8;
constexpr std::int32_t kFirstPoseAddedIn1_20_3 = 15;
constexpr std::int32_t kNitwit = 5;

[[nodiscard]] std::int32_t oldProfession(std::int32_t profession) {
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
        return 5;
    default:
        return kNitwit;
    }
}

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
                return kNone;
            } else {
                return static_cast<std::int32_t>(held.registry);
            }
        },
        value);
}

void writeComponent(io::ByteBuffer& output, const text::Component& component, bool asLegacyName, std::int32_t farEnd) {
    if (asLegacyName) {
        protocol::writeString(output, text::toPlainText(component));
    } else if (farEnd >= protocolVersions::kV1_20_3) {
        protocol::writeAnonymousNbt(output, text::componentNbt::serialize(component));
    } else {
        protocol::writeString(output, common::writeJson(text::componentJson::serialize(component)));
    }
}

} // namespace

EntityDataRewriter::EntityDataRewriter(std::int32_t farEnd, std::vector<std::int32_t> links, StackMapper stack)
    : farEnd_(farEnd), links_(std::move(links)), stack_(std::move(stack)), types_(entityDataTypes(farEnd)),
      packed_(farEnd < protocolVersions::kV1_9) {}

std::optional<std::vector<std::uint8_t>> EntityDataRewriter::rewrite(const SetEntityDataPacket& data,
                                                                     std::int32_t nativeType) const {
    if (types_.empty()) {
        return std::nullopt;
    }
    io::ByteBuffer output;
    protocol::varInt::write(output, data.entityId);
    for (const SetEntityDataPacket::Entry& entry : data.entries) {
        const std::int32_t index = indexFor(nativeType, entry.index);
        if (index >= 0) {
            writeEntry(output, entry, index, types_);
        }
    }
    output.writeByte(packed_ ? kEndOfPackedMetadata : kEndOfMetadata);
    return output.toVector();
}

std::int32_t EntityDataRewriter::indexFor(std::int32_t nativeType, std::int32_t nativeIndex) const {
    std::int32_t index = nativeIndex;
    for (const std::int32_t link : links_) {
        for (const EntityDataIndexRule& rule : entityDataIndexRules(link)) {
            if (nativeType < 0 || std::ranges::find(rule.types, nativeType) == rule.types.end()) {
                continue;
            }
            if (index == rule.index) {
                return kNone;
            }
            if (rule.shift && index > rule.index) {
                index--;
            }
        }
        const std::span<const EntityDataIndexMove> moves = entityDataIndexMoves(link);
        if (moves.empty()) {
            continue;
        }
        if (nativeType < 0) {
            return kNone;
        }
        const auto found = std::ranges::find_if(moves, [&](const EntityDataIndexMove& move) {
            return move.from == index && std::ranges::find(move.types, nativeType) != move.types.end();
        });
        if (found == moves.end()) {
            return kNone;
        }
        index = found->to;
    }
    return index;
}

void EntityDataRewriter::writeEntry(io::ByteBuffer& output, const SetEntityDataPacket::Entry& source,
                                    std::int32_t index, std::span<const std::int32_t> types) const {
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
            value = SetEntityDataPacket::IntValue{oldProfession(villager->professionId)};
        }
    }

    std::int32_t mapped = typeAt(types, nativeTypeOf(value));
    const bool asLegacyName = mapped == kNone && std::holds_alternative<SetEntityDataPacket::OptionalTextValue>(value);
    if (asLegacyName) {
        mapped = typeAt(types, kString);
    }
    const bool asByte = mapped == kNone && std::holds_alternative<SetEntityDataPacket::BoolValue>(value);
    if (asByte) {
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
                protocol::varInt::write(payload, held.value);
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
                written = false;
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
    if (!written) {
        return;
    }

    if (packed_) {
        if (index > kPackedIndexMask || mapped > kPackedTypeLimit) {
            return;
        }
        output.writeByte(static_cast<std::uint8_t>((mapped << kPackedTypeShift) | index));
    } else {
        output.writeByte(static_cast<std::uint8_t>(index));
        protocol::varInt::write(output, mapped);
    }
    output.writeBytes(payload.readable());
}

} // namespace basalt::wire::multiversion::java
