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

struct SetEquipmentPacket {
    static constexpr std::int32_t kId = 0x68;

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

struct TakeItemEntityPacket {
    static constexpr std::int32_t kId = 0x7f;

    std::int32_t collectedEntityId = 0;
    std::int32_t collectorEntityId = 0;
    std::int32_t pickupItemCount = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<TakeItemEntityPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const TakeItemEntityPacket&, const TakeItemEntityPacket&) = default;
};

struct LevelEventPacket {
    static constexpr std::int32_t kId = 0x2f;
    /// parity: LevelEvent.PARTICLES_DESTROY_BLOCK.
    static constexpr std::int32_t kParticlesDestroyBlock = 2001;

    std::int32_t event = 0;
    BlockPos pos;
    std::int32_t data = 0;
    bool global = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<LevelEventPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const LevelEventPacket&, const LevelEventPacket&) = default;
};

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

struct RotateHeadPacket {
    static constexpr std::int32_t kId = 0x55;

    std::int32_t entityId = 0;
    float headYaw = 0.0F;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RotateHeadPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const RotateHeadPacket&, const RotateHeadPacket&) = default;
};

struct AnimatePacket {
    static constexpr std::int32_t kId = 0x02;
    static constexpr std::uint8_t kWakeUp = 0;
    static constexpr std::uint8_t kCriticalHit = 1;
    static constexpr std::uint8_t kMagicCriticalHit = 2;

    std::int32_t entityId = 0;
    std::uint8_t action = kWakeUp;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<AnimatePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const AnimatePacket&, const AnimatePacket&) = default;
};

/// parity: `SwingAnimationType`.
enum class SwingAnimationType : std::uint8_t { None, Whack, Stab };

/// parity: `SwingAnimation`.
struct SwingAnimation {
    SwingAnimationType type = SwingAnimationType::Whack;
    std::int32_t duration = 6;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SwingAnimation> read(io::ByteBuffer& buffer);

    friend bool operator==(const SwingAnimation&, const SwingAnimation&) = default;
};

struct SwingAnimationPacket {
    static constexpr std::int32_t kId = 0x7b;

    std::int32_t entityId = 0;
    std::int32_t hand = 0;
    SwingAnimation animation;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SwingAnimationPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SwingAnimationPacket&, const SwingAnimationPacket&) = default;
};

struct SetEntityLinkPacket {
    static constexpr std::int32_t kId = 0x66;
    /// What vanilla writes to say "nothing", which is how a dismount is expressed.
    static constexpr std::int32_t kNoVehicle = -1;

    std::int32_t sourceId = 0;
    std::int32_t destinationId = kNoVehicle;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetEntityLinkPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetEntityLinkPacket&, const SetEntityLinkPacket&) = default;
};

/// The type goes first, with its own options.
struct LevelParticlesPacket {
    static constexpr std::int32_t kId = 0x30;

    /// parity: `ClientboundLevelParticlesPacket.RandomizationType`.
    enum class Randomization : std::uint8_t { Default, Alternative, AlternativeWithSpeed };

    std::int32_t particleTypeId = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float offsetX = 0.0F;
    float offsetY = 0.0F;
    float offsetZ = 0.0F;
    float maxSpeedX = 0.0F;
    float maxSpeedY = 0.0F;
    float maxSpeedZ = 0.0F;
    std::int32_t count = 1;
    /// parity: `force`.
    bool overrideLimiter = false;
    bool alwaysShow = false;
    Randomization randomization = Randomization::Default;
    std::optional<ParticleOptions> options;

    /// The same speed on the three axes, which is what every vanilla caller but a few passes.
    void setMaxSpeed(float speed) noexcept { maxSpeedX = maxSpeedY = maxSpeedZ = speed; }

    void write(io::ByteBuffer& buffer) const;

    /// Which options a type carries is the registry's business, so the shape comes from the caller.
    [[nodiscard]] static std::optional<LevelParticlesPacket> read(io::ByteBuffer& buffer, ParticleOptionShape shape);

    friend bool operator==(const LevelParticlesPacket&, const LevelParticlesPacket&) = default;
};

/// parity: ClientboundEntityEventPacket.
struct EntityEventPacket {
    static constexpr std::int32_t kId = 0x22;
    static constexpr std::int8_t kPermissionLevelBase = 24;
    /// parity: EntityEvent.DEATH.
    static constexpr std::int8_t kLivingDeath = 3;

    std::int32_t entityId = 0;
    std::int8_t eventId = 0;

    [[nodiscard]] static EntityEventPacket permissionLevel(std::int32_t entityId, std::int32_t level);

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<EntityEventPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const EntityEventPacket&, const EntityEventPacket&) = default;
};

struct TrackedWaypointPacket {
    static constexpr std::int32_t kId = 0x8d;
    /// parity: TrackedWaypoint.Type.
    static constexpr std::int32_t kWaypointTypeVec3i = 1;

    enum class Operation : std::uint8_t {
        Track,
        Untrack,
        Update,
    };

    Operation operation = Operation::Track;
    common::Uuid entityUuid;
    std::string style;
    /// parity: ByteBufCodecs.RGB_COLOR is three bytes, not an int.
    std::optional<std::int32_t> color;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<TrackedWaypointPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const TrackedWaypointPacket&, const TrackedWaypointPacket&) = default;
};

struct UpdateAttributesPacket {
    static constexpr std::int32_t kId = 0x86;

    /// parity: AttributeModifier.Operation.
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

struct DamageEventPacket {
    static constexpr std::int32_t kId = 0x19;

    std::int32_t entityId = 0;
    std::int32_t damageType = 0;
    std::int32_t causeEntityId = -1;
    std::int32_t directEntityId = -1;
    std::optional<double> sourceX;
    double sourceY = 0.0;
    double sourceZ = 0.0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<DamageEventPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const DamageEventPacket&, const DamageEventPacket&) = default;
};

/// parity: the effect id travels bare, because MobEffect.STREAM_CODEC is holderRegistry and not.
struct UpdateMobEffectPacket {
    static constexpr std::int32_t kId = 0x87;

    std::int32_t entityId = 0;
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

struct RemoveMobEffectPacket {
    static constexpr std::int32_t kId = 0x4f;

    std::int32_t entityId = 0;
    std::int32_t effect = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RemoveMobEffectPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const RemoveMobEffectPacket&, const RemoveMobEffectPacket&) = default;
};

/// parity: `PositionPath`.
struct EntityPositionSyncPacket {
    static constexpr std::int32_t kId = 0x23;

    std::int32_t entityId = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    bool onGround = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<EntityPositionSyncPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const EntityPositionSyncPacket&, const EntityPositionSyncPacket&) = default;
};

struct InteractPacket {
    static constexpr std::int32_t kId = 0x1a;
    /// parity: InteractionHand.
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

struct AttackPacket {
    static constexpr std::int32_t kId = 0x01;

    std::int32_t entityId = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<AttackPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const AttackPacket&, const AttackPacket&) = default;
};

struct SetEntityMotionPacket {
    static constexpr std::int32_t kId = 0x67;

    std::int32_t entityId = 0;
    double velocityX = 0.0;
    double velocityY = 0.0;
    double velocityZ = 0.0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetEntityMotionPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetEntityMotionPacket&, const SetEntityMotionPacket&) = default;
};

/// parity: ServerPlayer.indicateDamage.
struct HurtAnimationPacket {
    static constexpr std::int32_t kId = 0x2b;

    std::int32_t entityId = 0;
    float yaw = 0.0F;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<HurtAnimationPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const HurtAnimationPacket&, const HurtAnimationPacket&) = default;
};

struct SetPassengersPacket {
    static constexpr std::int32_t kId = 0x6d;

    std::int32_t vehicleId = 0;
    std::vector<std::int32_t> passengerIds;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetPassengersPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetPassengersPacket&, const SetPassengersPacket&) = default;
};

} // namespace basalt::wire::protocol::play
