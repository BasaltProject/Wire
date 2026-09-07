#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/LowPrecisionVec3.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace basalt::wire::protocol::play {
namespace {

constexpr std::uint8_t kEquipmentContinueMask = 0x80;

constexpr std::uint8_t kEffectFlagAmbient = 1;
constexpr std::uint8_t kEffectFlagVisible = 2;
constexpr std::uint8_t kEffectFlagShowIcon = 4;
constexpr std::uint8_t kEffectFlagBlend = 8;

template <typename Enum>
std::optional<Enum> fromOrdinal(std::int32_t ordinal, std::size_t count) noexcept {
    if (ordinal < 0 || static_cast<std::size_t>(ordinal) >= count) {
        return std::nullopt;
    }
    return static_cast<Enum>(ordinal);
}

} // namespace

void SetEquipmentPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const std::uint8_t continuation = i + 1 < slots.size() ? kEquipmentContinueMask : 0;
        buffer.writeByte(static_cast<std::uint8_t>(static_cast<std::uint8_t>(slots[i].type) | continuation));
        WireItemStack::writeOptional(buffer, slots[i].stack);
    }
}

std::optional<SetEquipmentPacket> SetEquipmentPacket::read(io::ByteBuffer& buffer) {
    SetEquipmentPacket packet;
    packet.entityId = varInt::read(buffer);

    std::uint8_t encoded = 0;
    do {
        encoded = buffer.readByte();
        if (buffer.failed()) {
            return std::nullopt;
        }
        const auto type = fromOrdinal<SlotType>(encoded & 0x7F, 8);
        if (!type) {
            buffer.markFailed();
            return std::nullopt;
        }
        packet.slots.push_back(Slot{*type, WireItemStack::read(buffer)});
    } while ((encoded & kEquipmentContinueMask) != 0 && !buffer.failed());

    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void TakeItemEntityPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, collectedEntityId);
    varInt::write(buffer, collectorEntityId);
    varInt::write(buffer, pickupItemCount);
}

std::optional<TakeItemEntityPacket> TakeItemEntityPacket::read(io::ByteBuffer& buffer) {
    TakeItemEntityPacket packet{varInt::read(buffer), varInt::read(buffer), varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void LevelEventPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeInt(event);
    buffer.writeLong(pos.pack());
    buffer.writeInt(data);
    writeBoolean(buffer, global);
}

std::optional<LevelEventPacket> LevelEventPacket::read(io::ByteBuffer& buffer) {
    LevelEventPacket packet;
    packet.event = buffer.readInt();
    packet.pos = BlockPos::unpack(buffer.readLong());
    packet.data = buffer.readInt();
    packet.global = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void BlockDestructionPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    buffer.writeLong(pos.pack());
    buffer.writeByte(stage);
}

std::optional<BlockDestructionPacket> BlockDestructionPacket::read(io::ByteBuffer& buffer) {
    BlockDestructionPacket packet;
    packet.entityId = varInt::read(buffer);
    packet.pos = BlockPos::unpack(buffer.readLong());
    packet.stage = buffer.readByte();
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void RotateHeadPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    buffer.writeByte(static_cast<std::uint8_t>(packDegrees(headYaw)));
}

std::optional<RotateHeadPacket> RotateHeadPacket::read(io::ByteBuffer& buffer) {
    RotateHeadPacket packet;
    packet.entityId = varInt::read(buffer);
    packet.headYaw = unpackDegrees(static_cast<std::int8_t>(buffer.readByte()));
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void AnimatePacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    buffer.writeByte(action);
}

std::optional<AnimatePacket> AnimatePacket::read(io::ByteBuffer& buffer) {
    AnimatePacket packet{varInt::read(buffer), buffer.readByte()};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SetEntityLinkPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeInt(sourceId);
    buffer.writeInt(destinationId);
}

std::optional<SetEntityLinkPacket> SetEntityLinkPacket::read(io::ByteBuffer& buffer) {
    SetEntityLinkPacket packet{buffer.readInt(), buffer.readInt()};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void LevelParticlesPacket::write(io::ByteBuffer& buffer) const {
    writeBoolean(buffer, overrideLimiter);
    writeBoolean(buffer, alwaysShow);
    buffer.writeDouble(x);
    buffer.writeDouble(y);
    buffer.writeDouble(z);
    buffer.writeFloat(offsetX);
    buffer.writeFloat(offsetY);
    buffer.writeFloat(offsetZ);
    buffer.writeFloat(maxSpeed);
    buffer.writeInt(count);
    varInt::write(buffer, particleTypeId);
    if (options) {
        options->write(buffer);
    }
}

std::optional<LevelParticlesPacket> LevelParticlesPacket::read(io::ByteBuffer& buffer, ParticleOptionShape shape) {
    LevelParticlesPacket packet;
    packet.overrideLimiter = readBoolean(buffer);
    packet.alwaysShow = readBoolean(buffer);
    packet.x = buffer.readDouble();
    packet.y = buffer.readDouble();
    packet.z = buffer.readDouble();
    packet.offsetX = buffer.readFloat();
    packet.offsetY = buffer.readFloat();
    packet.offsetZ = buffer.readFloat();
    packet.maxSpeed = buffer.readFloat();
    packet.count = buffer.readInt();
    packet.particleTypeId = varInt::read(buffer);
    if (shape != ParticleOptionShape::None) {
        packet.options = ParticleOptions::read(buffer, shape);
        if (!packet.options) {
            return std::nullopt;
        }
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

EntityEventPacket EntityEventPacket::permissionLevel(std::int32_t entityId, std::int32_t level) {
    const std::int32_t clamped = std::clamp(level, 0, 4);
    return EntityEventPacket{entityId, static_cast<std::int8_t>(kPermissionLevelBase + clamped)};
}

void EntityEventPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeInt(entityId);
    buffer.writeByte(static_cast<std::uint8_t>(eventId));
}

std::optional<EntityEventPacket> EntityEventPacket::read(io::ByteBuffer& buffer) {
    EntityEventPacket packet{buffer.readInt(), static_cast<std::int8_t>(buffer.readByte())};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void TrackedWaypointPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(operation));
    // True selects the UUID side of the either.
    writeBoolean(buffer, true);
    writeUuid(buffer, entityUuid);
    writeString(buffer, style);

    writeBoolean(buffer, color.has_value());
    if (color) {
        buffer.writeByte(static_cast<std::uint8_t>((*color >> 16) & 0xFF));
        buffer.writeByte(static_cast<std::uint8_t>((*color >> 8) & 0xFF));
        buffer.writeByte(static_cast<std::uint8_t>(*color & 0xFF));
    }

    varInt::write(buffer, kWaypointTypeVec3i);
    varInt::write(buffer, x);
    varInt::write(buffer, y);
    varInt::write(buffer, z);
}

std::optional<TrackedWaypointPacket> TrackedWaypointPacket::read(io::ByteBuffer& buffer) {
    TrackedWaypointPacket packet;
    const auto operation = fromOrdinal<Operation>(varInt::read(buffer), 3);
    if (!operation || !readBoolean(buffer)) {
        // Only the UUID side of the identifier is written here.
        buffer.markFailed();
        return std::nullopt;
    }
    packet.operation = *operation;
    packet.entityUuid = readUuid(buffer);
    packet.style = readString(buffer);

    if (readBoolean(buffer)) {
        const std::int32_t red = buffer.readByte();
        const std::int32_t green = buffer.readByte();
        const std::int32_t blue = buffer.readByte();
        packet.color = (red << 16) | (green << 8) | blue;
    }

    if (varInt::read(buffer) != kWaypointTypeVec3i) {
        buffer.markFailed();
        return std::nullopt;
    }
    packet.x = varInt::read(buffer);
    packet.y = varInt::read(buffer);
    packet.z = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void UpdateAttributesPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    varInt::write(buffer, static_cast<std::int32_t>(attributes.size()));
    for (const AttributeSnapshot& attribute : attributes) {
        varInt::write(buffer, attribute.attribute);
        buffer.writeDouble(attribute.base);
        varInt::write(buffer, static_cast<std::int32_t>(attribute.modifiers.size()));
        for (const Modifier& modifier : attribute.modifiers) {
            writeString(buffer, modifier.id.toString());
            buffer.writeDouble(modifier.amount);
            varInt::write(buffer, static_cast<std::int32_t>(modifier.operation));
        }
    }
}

std::optional<UpdateAttributesPacket> UpdateAttributesPacket::read(io::ByteBuffer& buffer) {
    UpdateAttributesPacket packet;
    packet.entityId = varInt::read(buffer);

    const std::int32_t count = readCollectionSize(buffer, 10);
    packet.attributes.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        AttributeSnapshot snapshot;
        snapshot.attribute = varInt::read(buffer);
        snapshot.base = buffer.readDouble();

        const std::int32_t modifiers = readCollectionSize(buffer, 10);
        snapshot.modifiers.reserve(static_cast<std::size_t>(modifiers));
        for (std::int32_t k = 0; k < modifiers && !buffer.failed(); ++k) {
            // Read in wire order as separate statements: folding them into one expression leaves
            // the order to the compiler, and the wire has only one.
            auto id = common::Identifier::parse(readString(buffer));
            const double amount = buffer.readDouble();
            const auto operation = fromOrdinal<Operation>(varInt::read(buffer), 3);
            if (!id || !operation) {
                buffer.markFailed();
                return std::nullopt;
            }
            snapshot.modifiers.push_back(Modifier{std::move(*id), amount, *operation});
        }
        packet.attributes.push_back(std::move(snapshot));
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void DamageEventPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    // A holder: zero would mean the type follows inline, so a registry id is written as id plus one.
    varInt::write(buffer, damageType + 1);
    varInt::write(buffer, causeEntityId + 1);
    varInt::write(buffer, directEntityId + 1);
    writeBoolean(buffer, sourceX.has_value());
    if (sourceX) {
        buffer.writeDouble(*sourceX);
        buffer.writeDouble(sourceY);
        buffer.writeDouble(sourceZ);
    }
}

std::optional<DamageEventPacket> DamageEventPacket::read(io::ByteBuffer& buffer) {
    DamageEventPacket packet;
    packet.entityId = varInt::read(buffer);
    packet.damageType = varInt::read(buffer) - 1;
    packet.causeEntityId = varInt::read(buffer) - 1;
    packet.directEntityId = varInt::read(buffer) - 1;
    if (readBoolean(buffer)) {
        packet.sourceX = buffer.readDouble();
        packet.sourceY = buffer.readDouble();
        packet.sourceZ = buffer.readDouble();
    }
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void UpdateMobEffectPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    varInt::write(buffer, effect);
    varInt::write(buffer, amplifier);
    varInt::write(buffer, durationTicks);

    std::uint8_t flags = 0;
    if (ambient) {
        flags |= kEffectFlagAmbient;
    }
    if (visible) {
        flags |= kEffectFlagVisible;
    }
    if (showIcon) {
        flags |= kEffectFlagShowIcon;
    }
    if (blend) {
        flags |= kEffectFlagBlend;
    }
    buffer.writeByte(flags);
}

std::optional<UpdateMobEffectPacket> UpdateMobEffectPacket::read(io::ByteBuffer& buffer) {
    UpdateMobEffectPacket packet;
    packet.entityId = varInt::read(buffer);
    packet.effect = varInt::read(buffer);
    packet.amplifier = varInt::read(buffer);
    packet.durationTicks = varInt::read(buffer);

    const std::uint8_t flags = buffer.readByte();
    packet.ambient = (flags & kEffectFlagAmbient) != 0;
    packet.visible = (flags & kEffectFlagVisible) != 0;
    packet.showIcon = (flags & kEffectFlagShowIcon) != 0;
    packet.blend = (flags & kEffectFlagBlend) != 0;
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void RemoveMobEffectPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    varInt::write(buffer, effect);
}

std::optional<RemoveMobEffectPacket> RemoveMobEffectPacket::read(io::ByteBuffer& buffer) {
    RemoveMobEffectPacket packet{varInt::read(buffer), varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void EntityPositionSyncPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    buffer.writeDouble(x);
    buffer.writeDouble(y);
    buffer.writeDouble(z);
    buffer.writeDouble(velocityX);
    buffer.writeDouble(velocityY);
    buffer.writeDouble(velocityZ);
    buffer.writeFloat(yaw);
    buffer.writeFloat(pitch);
    writeBoolean(buffer, onGround);
}

std::optional<EntityPositionSyncPacket> EntityPositionSyncPacket::read(io::ByteBuffer& buffer) {
    EntityPositionSyncPacket packet;
    packet.entityId = varInt::read(buffer);
    packet.x = buffer.readDouble();
    packet.y = buffer.readDouble();
    packet.z = buffer.readDouble();
    packet.velocityX = buffer.readDouble();
    packet.velocityY = buffer.readDouble();
    packet.velocityZ = buffer.readDouble();
    packet.yaw = buffer.readFloat();
    packet.pitch = buffer.readFloat();
    packet.onGround = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void InteractPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    varInt::write(buffer, hand);
    lowPrecisionVec3::write(buffer, x, y, z);
    writeBoolean(buffer, usingSecondaryAction);
}

std::optional<InteractPacket> InteractPacket::read(io::ByteBuffer& buffer) {
    InteractPacket packet;
    packet.entityId = varInt::read(buffer);
    packet.hand = varInt::read(buffer);
    const std::array<double, 3> location = lowPrecisionVec3::read(buffer);
    packet.x = location[0];
    packet.y = location[1];
    packet.z = location[2];
    packet.usingSecondaryAction = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void AttackPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, entityId); }

std::optional<AttackPacket> AttackPacket::read(io::ByteBuffer& buffer) {
    AttackPacket packet{varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SetEntityMotionPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    lowPrecisionVec3::write(buffer, velocityX, velocityY, velocityZ);
}

std::optional<SetEntityMotionPacket> SetEntityMotionPacket::read(io::ByteBuffer& buffer) {
    SetEntityMotionPacket packet;
    packet.entityId = varInt::read(buffer);
    const std::array<double, 3> velocity = lowPrecisionVec3::read(buffer);
    packet.velocityX = velocity[0];
    packet.velocityY = velocity[1];
    packet.velocityZ = velocity[2];
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void HurtAnimationPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    buffer.writeFloat(yaw);
}

std::optional<HurtAnimationPacket> HurtAnimationPacket::read(io::ByteBuffer& buffer) {
    HurtAnimationPacket packet{varInt::read(buffer), buffer.readFloat()};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SetPassengersPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, vehicleId);
    varInt::write(buffer, static_cast<std::int32_t>(passengerIds.size()));
    for (const std::int32_t id : passengerIds) {
        varInt::write(buffer, id);
    }
}

std::optional<SetPassengersPacket> SetPassengersPacket::read(io::ByteBuffer& buffer) {
    SetPassengersPacket packet;
    packet.vehicleId = varInt::read(buffer);
    const std::int32_t count = readCollectionSize(buffer);
    packet.passengerIds.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        packet.passengerIds.push_back(varInt::read(buffer));
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::play
