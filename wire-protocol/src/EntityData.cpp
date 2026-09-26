#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/LowPrecisionVec3.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <array>
#include <cmath>
#include <cstdint>

namespace basalt::wire::protocol::play {
namespace {

// parity: EntityDataSerializers' own registration ids. Only the ones an entity this server sends
// actually uses are here; anything else is refused rather than skipped, because a serializer whose
// width is unknown cannot be stepped over without losing the rest of the stream.
constexpr std::int32_t kByteSerializer = 0;
constexpr std::int32_t kIntSerializer = 1;
constexpr std::int32_t kLongSerializer = 2;
constexpr std::int32_t kFloatSerializer = 3;
constexpr std::int32_t kComponentSerializer = 5;
constexpr std::int32_t kOptionalComponentSerializer = 6;
constexpr std::int32_t kItemStackSerializer = 7;
constexpr std::int32_t kBooleanSerializer = 8;
constexpr std::int32_t kRotationsSerializer = 9;
constexpr std::int32_t kOptionalBlockPosSerializer = 11;
constexpr std::int32_t kParticleSerializer = 16;
constexpr std::int32_t kParticlesSerializer = 17;
/// A guard against a hostile count, far above the few dozen effects that exist.
constexpr std::int32_t kMaxParticles = 1024;
constexpr std::int32_t kVillagerDataSerializer = 18;
constexpr std::int32_t kOptionalUnsignedIntSerializer = 19;
constexpr std::int32_t kPoseSerializer = 20;
constexpr std::uint8_t kEndOfMetadata = 0xFF;

std::optional<RegistryHolder> registryHolderFromSerializer(std::int32_t id) noexcept {
    switch (id) {
    case 12:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 42:
    case 43:
        return static_cast<RegistryHolder>(id);
    default:
        return std::nullopt;
    }
}

void writeComponent(io::ByteBuffer& buffer, const text::Component& component) {
    writeAnonymousNbt(buffer, text::componentNbt::serialize(component));
}

text::Component readComponent(io::ByteBuffer& buffer) {
    return text::componentNbt::deserialize(readAnonymousNbt(buffer));
}

} // namespace

std::int8_t packDegrees(float value) noexcept {
    return static_cast<std::int8_t>(static_cast<std::int32_t>(std::floor(value * 256.0F / 360.0F)));
}

float unpackDegrees(std::int8_t value) noexcept { return static_cast<float>(value) * 360.0F / 256.0F; }

void SpawnEntityPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    writeUuid(buffer, uuid);
    varInt::write(buffer, entityType);
    buffer.writeDouble(x);
    buffer.writeDouble(y);
    buffer.writeDouble(z);
    lowPrecisionVec3::write(buffer, velocityX, velocityY, velocityZ);
    buffer.writeByte(static_cast<std::uint8_t>(packDegrees(pitch)));
    buffer.writeByte(static_cast<std::uint8_t>(packDegrees(yaw)));
    buffer.writeByte(static_cast<std::uint8_t>(packDegrees(headYaw)));
    varInt::write(buffer, objectData);
}

std::optional<SpawnEntityPacket> SpawnEntityPacket::read(io::ByteBuffer& buffer) {
    SpawnEntityPacket packet;
    packet.entityId = varInt::read(buffer);
    packet.uuid = readUuid(buffer);
    packet.entityType = varInt::read(buffer);
    packet.x = buffer.readDouble();
    packet.y = buffer.readDouble();
    packet.z = buffer.readDouble();

    const std::array<double, 3> velocity = lowPrecisionVec3::read(buffer);
    packet.velocityX = velocity[0];
    packet.velocityY = velocity[1];
    packet.velocityZ = velocity[2];

    packet.pitch = unpackDegrees(static_cast<std::int8_t>(buffer.readByte()));
    packet.yaw = unpackDegrees(static_cast<std::int8_t>(buffer.readByte()));
    packet.headYaw = unpackDegrees(static_cast<std::int8_t>(buffer.readByte()));
    packet.objectData = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SetEntityDataPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);

    for (const Entry& entry : entries) {
        buffer.writeByte(entry.index);
        if (const auto* byteValue = std::get_if<ByteValue>(&entry.value)) {
            varInt::write(buffer, kByteSerializer);
            buffer.writeByte(static_cast<std::uint8_t>(byteValue->value));
        } else if (const auto* intValue = std::get_if<IntValue>(&entry.value)) {
            varInt::write(buffer, kIntSerializer);
            varInt::write(buffer, intValue->value);
        } else if (const auto* floatValue = std::get_if<FloatValue>(&entry.value)) {
            varInt::write(buffer, kFloatSerializer);
            buffer.writeFloat(floatValue->value);
        } else if (const auto* textValue = std::get_if<TextValue>(&entry.value)) {
            varInt::write(buffer, kComponentSerializer);
            writeComponent(buffer, textValue->value);
        } else if (const auto* optionalText = std::get_if<OptionalTextValue>(&entry.value)) {
            varInt::write(buffer, kOptionalComponentSerializer);
            writeBoolean(buffer, optionalText->value.has_value());
            if (optionalText->value) {
                writeComponent(buffer, *optionalText->value);
            }
        } else if (const auto* itemValue = std::get_if<ItemValue>(&entry.value)) {
            varInt::write(buffer, kItemStackSerializer);
            WireItemStack::writeOptional(buffer, itemValue->stack);
        } else if (const auto* boolValue = std::get_if<BoolValue>(&entry.value)) {
            varInt::write(buffer, kBooleanSerializer);
            writeBoolean(buffer, boolValue->value);
        } else if (const auto* rotations = std::get_if<RotationsValue>(&entry.value)) {
            varInt::write(buffer, kRotationsSerializer);
            buffer.writeFloat(rotations->x);
            buffer.writeFloat(rotations->y);
            buffer.writeFloat(rotations->z);
        } else if (const auto* villager = std::get_if<VillagerDataValue>(&entry.value)) {
            varInt::write(buffer, kVillagerDataSerializer);
            varInt::write(buffer, villager->typeId);
            varInt::write(buffer, villager->professionId);
            varInt::write(buffer, villager->level);
        } else if (const auto* blockPos = std::get_if<OptionalBlockPosValue>(&entry.value)) {
            varInt::write(buffer, kOptionalBlockPosSerializer);
            writeBoolean(buffer, blockPos->pos.has_value());
            if (blockPos->pos) {
                buffer.writeLong(blockPos->pos->pack());
            }
        } else if (const auto* poseValue = std::get_if<PoseValue>(&entry.value)) {
            varInt::write(buffer, kPoseSerializer);
            varInt::write(buffer, static_cast<std::int32_t>(poseValue->pose));
        } else if (const auto* particle = std::get_if<ParticleValue>(&entry.value)) {
            varInt::write(buffer, kParticleSerializer);
            varInt::write(buffer, particle->particleId);
            if (particle->options) {
                particle->options->write(buffer);
            }
        } else if (const auto* list = std::get_if<ParticlesValue>(&entry.value)) {
            varInt::write(buffer, kParticlesSerializer);
            varInt::write(buffer, static_cast<std::int32_t>(list->particles.size()));
            for (const ParticleValue& each : list->particles) {
                varInt::write(buffer, each.particleId);
                if (each.options) {
                    each.options->write(buffer);
                }
            }
        } else if (const auto* longValue = std::get_if<LongValue>(&entry.value)) {
            varInt::write(buffer, kLongSerializer);
            varLong::write(buffer, longValue->value);
        } else if (const auto* optionalInt = std::get_if<OptionalUnsignedIntValue>(&entry.value)) {
            varInt::write(buffer, kOptionalUnsignedIntSerializer);
            varInt::write(buffer, optionalInt->value.has_value() ? *optionalInt->value + 1 : 0);
        } else if (const auto* registryValue = std::get_if<RegistryIdValue>(&entry.value)) {
            varInt::write(buffer, static_cast<std::int32_t>(registryValue->registry));
            varInt::write(buffer, registryValue->value);
        }
    }

    buffer.writeByte(kEndOfMetadata);
}

std::optional<SetEntityDataPacket> SetEntityDataPacket::read(io::ByteBuffer& buffer, const ParticleShapeOf& shapeOf) {
    SetEntityDataPacket packet;
    packet.entityId = varInt::read(buffer);

    while (!buffer.failed()) {
        const std::uint8_t index = buffer.readByte();
        if (buffer.failed()) {
            return std::nullopt;
        }
        if (index == kEndOfMetadata) {
            return packet;
        }

        Entry entry;
        entry.index = index;
        const std::int32_t serializer = varInt::read(buffer);
        switch (serializer) {
        case kByteSerializer:
            entry.value = ByteValue{static_cast<std::int8_t>(buffer.readByte())};
            break;
        case kIntSerializer:
            entry.value = IntValue{varInt::read(buffer)};
            break;
        case kFloatSerializer:
            entry.value = FloatValue{buffer.readFloat()};
            break;
        case kComponentSerializer:
            entry.value = TextValue{readComponent(buffer)};
            break;
        case kOptionalComponentSerializer: {
            OptionalTextValue value;
            if (readBoolean(buffer)) {
                value.value = readComponent(buffer);
            }
            entry.value = std::move(value);
            break;
        }
        case kItemStackSerializer:
            entry.value = ItemValue{WireItemStack::read(buffer)};
            break;
        case kBooleanSerializer:
            entry.value = BoolValue{readBoolean(buffer)};
            break;
        case kRotationsSerializer: {
            RotationsValue value;
            value.x = buffer.readFloat();
            value.y = buffer.readFloat();
            value.z = buffer.readFloat();
            entry.value = value;
            break;
        }
        case kLongSerializer:
            entry.value = LongValue{varLong::read(buffer)};
            break;
        case kOptionalUnsignedIntSerializer: {
            const std::int32_t raw = varInt::read(buffer);
            entry.value = OptionalUnsignedIntValue{raw == 0 ? std::nullopt : std::optional{raw - 1}};
            break;
        }
        case kVillagerDataSerializer: {
            VillagerDataValue value;
            value.typeId = varInt::read(buffer);
            value.professionId = varInt::read(buffer);
            value.level = varInt::read(buffer);
            entry.value = value;
            break;
        }
        case kOptionalBlockPosSerializer: {
            OptionalBlockPosValue value;
            if (readBoolean(buffer)) {
                value.pos = BlockPos::unpack(buffer.readLong());
            }
            entry.value = value;
            break;
        }
        case kPoseSerializer: {
            const std::int32_t pose = varInt::read(buffer);
            if (pose < 0 || pose > static_cast<std::int32_t>(Pose::Inhaling)) {
                buffer.markFailed();
                return std::nullopt;
            }
            entry.value = PoseValue{static_cast<Pose>(pose)};
            break;
        }
        case kParticleSerializer: {
            if (!shapeOf) {
                buffer.markFailed();
                return std::nullopt;
            }
            ParticleValue value;
            value.particleId = varInt::read(buffer);
            if (const std::optional<ParticleOptionShape> shape = shapeOf(value.particleId);
                shape.has_value() && *shape != ParticleOptionShape::None) {
                value.options = ParticleOptions::read(buffer, *shape);
                if (!value.options) {
                    buffer.markFailed();
                    return std::nullopt;
                }
            }
            entry.value = std::move(value);
            break;
        }
        case kParticlesSerializer: {
            if (!shapeOf) {
                buffer.markFailed();
                return std::nullopt;
            }
            const std::int32_t count = varInt::read(buffer);
            if (count < 0 || count > kMaxParticles) {
                buffer.markFailed();
                return std::nullopt;
            }
            ParticlesValue list;
            for (std::int32_t read = 0; read < count && !buffer.failed(); ++read) {
                ParticleValue value;
                value.particleId = varInt::read(buffer);
                if (const std::optional<ParticleOptionShape> shape = shapeOf(value.particleId);
                    shape.has_value() && *shape != ParticleOptionShape::None) {
                    value.options = ParticleOptions::read(buffer, *shape);
                    if (!value.options) {
                        buffer.markFailed();
                        return std::nullopt;
                    }
                }
                list.particles.push_back(std::move(value));
            }
            entry.value = std::move(list);
            break;
        }
        default: {
            const std::optional<RegistryHolder> registry = registryHolderFromSerializer(serializer);
            if (!registry) {
                buffer.markFailed();
                return std::nullopt;
            }
            entry.value = RegistryIdValue{varInt::read(buffer), *registry};
            break;
        }
        }
        packet.entries.push_back(std::move(entry));
    }
    return std::nullopt;
}

SetEntityDataPacket SetEntityDataPacket::item(std::int32_t entityId, std::optional<WireItemStack> stack) {
    return SetEntityDataPacket{entityId, {Entry{kItemStackIndex, ItemValue{std::move(stack)}}}};
}

SetEntityDataPacket SetEntityDataPacket::itemFrame(std::int32_t entityId, std::optional<WireItemStack> stack) {
    return SetEntityDataPacket{entityId, {Entry{kItemFrameStackIndex, ItemValue{std::move(stack)}}}};
}

SetEntityDataPacket SetEntityDataPacket::submersion(std::int32_t entityId, std::int32_t airSupply, std::uint8_t flags) {
    return SetEntityDataPacket{entityId,
                               {Entry{kSharedFlagsIndex, ByteValue{static_cast<std::int8_t>(flags)}},
                                Entry{kAirSupplyIndex, IntValue{airSupply}}}};
}

SetEntityDataPacket SetEntityDataPacket::posture(std::int32_t entityId, std::uint8_t flags, Pose pose) {
    return SetEntityDataPacket{
        entityId,
        {Entry{kSharedFlagsIndex, ByteValue{static_cast<std::int8_t>(flags)}}, Entry{kPoseIndex, PoseValue{pose}}}};
}

SetEntityDataPacket SetEntityDataPacket::skinParts(std::int32_t entityId, std::int8_t skinParts) {
    return SetEntityDataPacket{entityId, {Entry{kPlayerSkinPartsIndex, ByteValue{skinParts}}}};
}

SetEntityDataPacket SetEntityDataPacket::sleeping(std::int32_t entityId, std::optional<BlockPos> bed) {
    const Pose pose = bed ? Pose::Sleeping : Pose::Standing;
    return SetEntityDataPacket{
        entityId, {Entry{kPoseIndex, PoseValue{pose}}, Entry{kSleepingPosIndex, OptionalBlockPosValue{bed}}}};
}

SetEntityDataPacket SetEntityDataPacket::playerAbsorption(std::int32_t entityId, float amount) {
    return SetEntityDataPacket{entityId, {Entry{kPlayerAbsorptionIndex, FloatValue{amount}}}};
}

} // namespace basalt::wire::protocol::play
