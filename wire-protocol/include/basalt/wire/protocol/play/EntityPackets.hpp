#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::protocol::play {

/// Clientbound, id 0x66: equipment changes. The high bit on every slot except the last one is the
/// continuation marker.
struct SetEquipmentPacket {
    static constexpr std::int32_t kId = 0x66;

    enum class SlotType : std::uint8_t {
        MainHand,
        OffHand,
        Feet,
        Legs,
        Chest,
        Head,
        Body,
        Saddle,
    };

    struct Slot {
        SlotType type = SlotType::MainHand;
        std::optional<WireItemStack> stack;

        friend bool operator==(const Slot&, const Slot&) = default;
    };

    std::int32_t entityId = 0;
    std::vector<Slot> slots;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetEquipmentPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetEquipmentPacket&, const SetEquipmentPacket&) = default;
};

/// Clientbound, id 0x7c: plays the "item flies to the player" pickup animation. Purely cosmetic: the
/// inventory change itself travels through the container packets sent alongside it.
struct TakeItemEntityPacket {
    static constexpr std::int32_t kId = 0x7c;

    std::int32_t collectedEntityId = 0;
    std::int32_t collectorEntityId = 0;
    std::int32_t pickupItemCount = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<TakeItemEntityPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const TakeItemEntityPacket&, const TakeItemEntityPacket&) = default;
};

/// Clientbound, id 0x2e: a one-off effect at a position.
///
/// The one this build sends is the block-break puff of particles and its sound: the breaker plays
/// its own, so without this everyone else sees blocks vanish in silence.
struct LevelEventPacket {
    static constexpr std::int32_t kId = 0x2e;
    /// parity: LevelEvent.PARTICLES_DESTROY_BLOCK, whose data is the broken block's state id.
    static constexpr std::int32_t kParticlesDestroyBlock = 2001;

    std::int32_t event = 0;
    BlockPos pos;
    std::int32_t data = 0;
    bool global = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<LevelEventPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const LevelEventPacket&, const LevelEventPacket&) = default;
};

/// Clientbound, id 0x05: the cracks on a block somebody is mining.
///
/// The stage is 0 to 9, and anything outside that range clears it: the digger predicts its own
/// cracks, and everyone else only sees them because of this.
struct BlockDestructionPacket {
    static constexpr std::int32_t kId = 0x05;
    /// parity: ServerPlayerGameMode's own range, and 10 as the "no cracks" value it sends.
    static constexpr std::uint8_t kStages = 10;
    static constexpr std::uint8_t kNone = 10;

    std::int32_t entityId = 0;
    BlockPos pos;
    std::uint8_t stage = kNone;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<BlockDestructionPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const BlockDestructionPacket&, const BlockDestructionPacket&) = default;
};

/// Clientbound, id 0x53: where an entity's head is looking.
///
/// The rotation packets carry the *body*, and a player's head is tracked apart from it: without
/// this, another player's head only tips up and down and never turns to look sideways.
struct RotateHeadPacket {
    static constexpr std::int32_t kId = 0x53;

    std::int32_t entityId = 0;
    float headYaw = 0.0F;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RotateHeadPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const RotateHeadPacket&, const RotateHeadPacket&) = default;
};

/// Clientbound, id 0x02: a one-off animation on an entity.
///
/// The action is a byte with no varint, and the values are vanilla's own: swinging the off hand is
/// 3, not 1, because 1 was the old hurt animation and the numbering was never compacted.
struct AnimatePacket {
    static constexpr std::int32_t kId = 0x02;
    static constexpr std::uint8_t kSwingMainHand = 0;
    static constexpr std::uint8_t kWakeUp = 2;
    static constexpr std::uint8_t kSwingOffHand = 3;
    static constexpr std::uint8_t kCriticalHit = 4;
    static constexpr std::uint8_t kMagicCriticalHit = 5;

    std::int32_t entityId = 0;
    std::uint8_t action = kSwingMainHand;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<AnimatePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const AnimatePacket&, const AnimatePacket&) = default;
};

/// Clientbound, id 0x64: who is leashed to, or riding, what.
///
/// Both ids are plain ints rather than varints, which is unusual enough to be worth stating: a
/// varint here writes a shorter frame and desynchronises the client on the next packet.
struct SetEntityLinkPacket {
    static constexpr std::int32_t kId = 0x64;
    /// What vanilla writes to say "nothing", which is how a dismount is expressed.
    static constexpr std::int32_t kNoVehicle = -1;

    std::int32_t sourceId = 0;
    std::int32_t destinationId = kNoVehicle;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetEntityLinkPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetEntityLinkPacket&, const SetEntityLinkPacket&) = default;
};

/// Clientbound, id 0x2f: particles at a position.
///
/// The particle *type* is written last, after the counts, and carries its own options for the types
/// that have them.
struct LevelParticlesPacket {
    static constexpr std::int32_t kId = 0x2f;

    std::int32_t particleTypeId = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float offsetX = 0.0F;
    float offsetY = 0.0F;
    float offsetZ = 0.0F;
    float maxSpeed = 0.0F;
    std::int32_t count = 1;
    /// parity: `force`, which shows the particle past the client's own particle-count setting.
    bool overrideLimiter = false;
    bool alwaysShow = false;
    /// The type's own extra data, written straight after its id. Absent for the types that carry
    /// none.
    std::optional<ParticleOptions> options;

    void write(io::ByteBuffer& buffer) const;

    /// The shape has to come from the caller: which options a type carries is the registry's
    /// business, and it is not on the wire.
    [[nodiscard]] static std::optional<LevelParticlesPacket> read(io::ByteBuffer& buffer, ParticleOptionShape shape);

    friend bool operator==(const LevelParticlesPacket&, const LevelParticlesPacket&) = default;
};

/// parity: ClientboundEntityEventPacket.
///
/// Most events are animations, but 24 to 28 are the player's own permission level, and the client
/// needs them: its local command parser refuses selectors until it has been told the player is at
/// least a gamemaster, so without this a perfectly valid command is drawn in red.
struct EntityEventPacket {
    static constexpr std::int32_t kId = 0x22;
    /// parity: EntityEvent.PERMISSION_LEVEL_ALL and the four above it.
    static constexpr std::int8_t kPermissionLevelBase = 24;
    /// parity: EntityEvent.DEATH, which is what plays the dying animation.
    static constexpr std::int8_t kLivingDeath = 3;

    std::int32_t entityId = 0;
    std::int8_t eventId = 0;

    [[nodiscard]] static EntityEventPacket permissionLevel(std::int32_t entityId, std::int32_t level);

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<EntityEventPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const EntityEventPacket&, const EntityEventPacket&) = default;
};

/// Clientbound, id 0x8a: one entry of the locator bar.
///
/// The identifier is an either of a UUID and a string, written as a boolean and then the chosen
/// side. Only the positioned kind is written here, which is the kind a command can point at.
struct TrackedWaypointPacket {
    static constexpr std::int32_t kId = 0x8a;
    /// parity: TrackedWaypoint.Type, where empty is 0, a position 1, a chunk 2 and an azimuth 3.
    static constexpr std::int32_t kWaypointTypeVec3i = 1;

    enum class Operation : std::uint8_t {
        Track,
        Untrack,
        Update,
    };

    Operation operation = Operation::Track;
    common::Uuid entityUuid;
    std::string style;
    /// parity: ByteBufCodecs.RGB_COLOR is three bytes, not an int. Writing four disconnects the
    /// client: it reads the waypoint type out of the leftover byte.
    std::optional<std::int32_t> color;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<TrackedWaypointPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const TrackedWaypointPacket&, const TrackedWaypointPacket&) = default;
};

/// Clientbound, id 0x83: an entity's attributes and the modifiers on them.
///
/// This is how a client learns about mining efficiency: it reads the attribute off its own map,
/// which only this packet fills, and not off the item in its hand.
struct UpdateAttributesPacket {
    static constexpr std::int32_t kId = 0x83;

    /// parity: AttributeModifier.Operation, in its own declared order.
    enum class Operation : std::uint8_t {
        AddValue,
        AddMultipliedBase,
        AddMultipliedTotal,
    };

    struct Modifier {
        common::Identifier id{"minecraft", "base"};
        double amount = 0.0;
        Operation operation = Operation::AddValue;

        friend bool operator==(const Modifier&, const Modifier&) = default;
    };

    struct AttributeSnapshot {
        /// The `minecraft:attribute` registry id.
        std::int32_t attribute = 0;
        double base = 0.0;
        std::vector<Modifier> modifiers;

        friend bool operator==(const AttributeSnapshot&, const AttributeSnapshot&) = default;
    };

    std::int32_t entityId = 0;
    std::vector<AttributeSnapshot> attributes;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<UpdateAttributesPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const UpdateAttributesPacket&, const UpdateAttributesPacket&) = default;
};

/// What made an entity flash red, and where the hit came from.
///
/// The damage type travels as a holder of the synced registry, and the two entity ids as "id plus
/// one", so zero means nobody.
struct DamageEventPacket {
    static constexpr std::int32_t kId = 0x19;

    std::int32_t entityId = 0;
    /// The index of the damage type in the `minecraft:damage_type` registry.
    std::int32_t damageType = 0;
    /// Whoever is blamed for the hit, or -1 for a hit nobody dealt.
    std::int32_t causeEntityId = -1;
    /// What actually touched them, an arrow say, or -1.
    std::int32_t directEntityId = -1;
    std::optional<double> sourceX;
    double sourceY = 0.0;
    double sourceZ = 0.0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<DamageEventPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const DamageEventPacket&, const DamageEventPacket&) = default;
};

/// Clientbound, id 0x84: one status effect on an entity.
///
/// parity: the effect id travels bare, because MobEffect.STREAM_CODEC is holderRegistry and not
/// holder, so there is no plus one here. The two are easy to confuse and the wrong one silently
/// shifts every effect.
struct UpdateMobEffectPacket {
    static constexpr std::int32_t kId = 0x84;

    std::int32_t entityId = 0;
    /// The `minecraft:mob_effect` registry id.
    std::int32_t effect = 0;
    std::int32_t amplifier = 0;
    std::int32_t durationTicks = 0;
    bool ambient = false;
    bool visible = true;
    bool showIcon = true;
    bool blend = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<UpdateMobEffectPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const UpdateMobEffectPacket&, const UpdateMobEffectPacket&) = default;
};

/// Clientbound, id 0x4e: one status effect gone.
struct RemoveMobEffectPacket {
    static constexpr std::int32_t kId = 0x4e;

    std::int32_t entityId = 0;
    std::int32_t effect = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RemoveMobEffectPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const RemoveMobEffectPacket&, const RemoveMobEffectPacket&) = default;
};

/// Clientbound, id 0x23: where an entity is, how fast it is going and which way it faces.
///
/// The velocity is the point of it: a client that knows it interpolates between updates, where a
/// teleport snaps. Vanilla sends this for anything whose position has to be exact, and a falling
/// block is one of them.
struct EntityPositionSyncPacket {
    static constexpr std::int32_t kId = 0x23;

    std::int32_t entityId = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double velocityX = 0.0;
    double velocityY = 0.0;
    double velocityZ = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    bool onGround = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<EntityPositionSyncPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const EntityPositionSyncPacket&, const EntityPositionSyncPacket&) = default;
};

/// Serverbound, id 0x1a: somebody right-clicked an entity.
///
/// This is how a player feeds an animal, opens a villager's trades, or gets on a boat. The point is
/// where on the entity the click landed, which is what tells sitting on a boat from riding it.
struct InteractPacket {
    static constexpr std::int32_t kId = 0x1a;
    /// parity: InteractionHand, the main hand and then the off hand.
    static constexpr std::int32_t kMainHand = 0;
    static constexpr std::int32_t kOffHand = 1;

    std::int32_t entityId = 0;
    std::int32_t hand = kMainHand;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool usingSecondaryAction = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<InteractPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const InteractPacket&, const InteractPacket&) = default;
};

/// Serverbound, id 0x01: somebody hit an entity, which 26.2 split out of the interact packet.
struct AttackPacket {
    static constexpr std::int32_t kId = 0x01;

    std::int32_t entityId = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<AttackPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const AttackPacket&, const AttackPacket&) = default;
};

/// Clientbound, id 0x65: how fast an entity is going now.
///
/// A client moves its own player, so a push it is not told about never happens: this is what makes
/// knockback, a piston or a bubble column move the person standing there.
struct SetEntityMotionPacket {
    static constexpr std::int32_t kId = 0x65;

    std::int32_t entityId = 0;
    double velocityX = 0.0;
    double velocityY = 0.0;
    double velocityZ = 0.0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetEntityMotionPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetEntityMotionPacket&, const SetEntityMotionPacket&) = default;
};

/// Clientbound, id 0x2a: the tilt of a hit.
///
/// parity: ServerPlayer.indicateDamage, where the angle is where the hit came from relative to the
/// way the player is looking, which is what leans the screen towards it.
struct HurtAnimationPacket {
    static constexpr std::int32_t kId = 0x2a;

    std::int32_t entityId = 0;
    float yaw = 0.0F;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<HurtAnimationPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const HurtAnimationPacket&, const HurtAnimationPacket&) = default;
};

/// Clientbound, id 0x6b: who is riding what.
///
/// Riding is not a position: a client draws the passenger on the vehicle because this says so, and
/// a player who mounted without it stays standing beside the cart on every other screen.
struct SetPassengersPacket {
    static constexpr std::int32_t kId = 0x6b;

    std::int32_t vehicleId = 0;
    std::vector<std::int32_t> passengerIds;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetPassengersPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetPassengersPacket&, const SetPassengersPacket&) = default;
};

} // namespace basalt::wire::protocol::play
