#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

namespace basalt::wire::protocol::play {

/// parity: Mth.packDegrees and unpackDegrees, a rotation as a single byte, 256 steps per full turn.
[[nodiscard]] std::int8_t packDegrees(float value) noexcept;

[[nodiscard]] float unpackDegrees(std::int8_t value) noexcept;

/// Clientbound, id 0x01: spawns an entity.
///
/// The entity type is the numeric protocol id of the type. The object data carries type-specific
/// extra, such as an item frame's direction, and is zero for the types that need none.
struct SpawnEntityPacket {
    static constexpr std::int32_t kId = 0x01;
    /// The protocol id of `minecraft:item` in the real 26.2 entity type registry.
    static constexpr std::int32_t kItemEntityType = 71;
    /// The protocol id of `minecraft:player`, read off the jar's own registries.json rather than a
    /// published table: minecraft-data still lists 155 for 26.1 and the entry moved, so a copied
    /// constant spawns the wrong entity entirely.
    static constexpr std::int32_t kPlayerEntityType = 156;

    std::int32_t entityId = 0;
    common::Uuid uuid;
    std::int32_t entityType = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double velocityX = 0.0;
    double velocityY = 0.0;
    double velocityZ = 0.0;
    float pitch = 0.0F;
    float yaw = 0.0F;
    float headYaw = 0.0F;
    std::int32_t objectData = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SpawnEntityPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SpawnEntityPacket&, const SpawnEntityPacket&) = default;
};

/// The registry-specific holder serializers, in 26.3's registration order, and the id-mapped enums
/// after them that travel the same way, as one VarInt.
enum class RegistryHolder : std::int32_t {
    Direction = 12,
    CatVariant = 21,
    CatSoundVariant = 22,
    CowVariant = 23,
    CowSoundVariant = 24,
    WolfVariant = 25,
    WolfSoundVariant = 26,
    FrogVariant = 27,
    PigVariant = 28,
    PigSoundVariant = 29,
    ChickenVariant = 30,
    ChickenSoundVariant = 31,
    ZombieNautilusVariant = 32,
    PaintingVariant = 34,
    SnifferState = 35,
    ArmadilloState = 36,
    CopperGolemState = 37,
    WeatheringCopperState = 38,
    DyeColor = 42,
};

/// Clientbound, id 0x65: entity metadata. A stream of entries, each an index byte, a serializer id
/// and the value, terminated by 0xFF.
///
/// Indices are positions in the entity's own defineId order, counted down the real class hierarchy,
/// which is why the shared ones are named rather than written as literals at call sites: 26.2 put a
/// new Avatar class between LivingEntity and Player, so the player indices are not where an older
/// version would put them.
struct SetEntityDataPacket {
    static constexpr std::int32_t kId = 0x65;

    /// ItemEntity.DATA_ITEM: Entity claims 0 to 7, so the item's own entry lands here.
    static constexpr std::uint8_t kItemStackIndex = 8;
    /// ItemFrame.DATA_ITEM: Entity 0 to 7, the hanging direction at 8, then the frame's item.
    static constexpr std::uint8_t kItemFrameStackIndex = 9;
    /// Avatar.DATA_PLAYER_MODE_CUSTOMISATION, the bitmask of which skin layers to draw. Entity 0 to
    /// 7, LivingEntity 8 to 14, Avatar 15 and 16.
    static constexpr std::uint8_t kPlayerSkinPartsIndex = 16;
    /// parity: Player.DATA_PLAYER_ABSORPTION_ID, the yellow hearts. 26.2 put Avatar between
    /// LivingEntity and Player, so this moved: it is not the 15 older versions used.
    static constexpr std::uint8_t kPlayerAbsorptionIndex = 17;
    /// parity: Entity.DATA_POSE, which is what draws a player lying in a bed.
    static constexpr std::uint8_t kPoseIndex = 6;
    /// parity: Entity.DATA_CUSTOM_NAME, the floating name, and whether it is drawn.
    static constexpr std::uint8_t kCustomNameIndex = 2;
    static constexpr std::uint8_t kCustomNameVisibleIndex = 3;
    /// Entity's own entries, which every entity has whatever it is.
    static constexpr std::uint8_t kLastSharedIndex = 7;
    /// parity: LivingEntity.SLEEPING_POS_ID, the bed they are lying in.
    static constexpr std::uint8_t kSleepingPosIndex = 14;
    /// parity: LivingEntity.DATA_LIVING_ENTITY_FLAGS, the first entry a living entity adds: bit one
    /// is "using an item", bit two says it is the off hand, bit three is riptide.
    static constexpr std::uint8_t kLivingFlagsIndex = 8;
    /// parity: Entity.DATA_SHARED_FLAGS_ID, whose lowest bit is "this entity is on fire".
    static constexpr std::uint8_t kSharedFlagsIndex = 0;
    /// parity: Entity.DATA_AIR_SUPPLY_ID, the ticks of air left, drawn as bubbles.
    static constexpr std::uint8_t kAirSupplyIndex = 1;

    /// parity: Entity's own bit numbers for the shared flags byte.
    static constexpr std::uint8_t kFlagOnFire = 0x01;
    static constexpr std::uint8_t kFlagSneaking = 0x02;
    static constexpr std::uint8_t kFlagSprinting = 0x08;
    static constexpr std::uint8_t kFlagSwimming = 0x10;
    static constexpr std::uint8_t kFlagInvisible = 0x20;
    static constexpr std::uint8_t kFlagGlowing = 0x40;
    static constexpr std::uint8_t kFlagFallFlying = 0x80;

    struct ByteValue {
        std::int8_t value = 0;
        friend bool operator==(const ByteValue&, const ByteValue&) = default;
    };

    struct IntValue {
        std::int32_t value = 0;
        friend bool operator==(const IntValue&, const IntValue&) = default;
    };

    struct FloatValue {
        float value = 0.0F;
        friend bool operator==(const FloatValue&, const FloatValue&) = default;
    };

    struct TextValue {
        text::Component value;
        friend bool operator==(const TextValue&, const TextValue&) = default;
    };

    /// The name over an entity's head. Absent is the name being taken off again, which is not the
    /// same as never having sent one.
    struct OptionalTextValue {
        std::optional<text::Component> value;
        friend bool operator==(const OptionalTextValue&, const OptionalTextValue&) = default;
    };

    struct ItemValue {
        std::optional<WireItemStack> stack;
        friend bool operator==(const ItemValue&, const ItemValue&) = default;
    };

    struct BoolValue {
        bool value = false;
        friend bool operator==(const BoolValue&, const BoolValue&) = default;
    };

    struct RotationsValue {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        friend bool operator==(const RotationsValue&, const RotationsValue&) = default;
    };

    struct VillagerDataValue {
        std::int32_t typeId = 0;
        std::int32_t professionId = 0;
        std::int32_t level = 1;
        friend bool operator==(const VillagerDataValue&, const VillagerDataValue&) = default;
    };

    struct OptionalBlockPosValue {
        std::optional<BlockPos> pos;
        friend bool operator==(const OptionalBlockPosValue&, const OptionalBlockPosValue&) = default;
    };

    struct PoseValue {
        Pose pose = Pose::Standing;
        friend bool operator==(const PoseValue&, const PoseValue&) = default;
    };

    /// A holder in a synchronized dynamic registry, as its varint id.
    struct RegistryIdValue {
        std::int32_t value = 0;
        RegistryHolder registry = RegistryHolder::CatVariant;
        friend bool operator==(const RegistryIdValue&, const RegistryIdValue&) = default;
    };

    /// A particle type and its options. parity: `ParticleTypes.STREAM_CODEC`, the id then the type's
    /// own codec.
    struct ParticleValue {
        std::int32_t particleId = 0;
        std::optional<ParticleOptions> options;
        friend bool operator==(const ParticleValue&, const ParticleValue&) = default;
    };

    /// A list of particles. parity: `EntityDataSerializers.PARTICLES`, a living entity's effect swirl.
    struct ParticlesValue {
        std::vector<ParticleValue> particles;
        friend bool operator==(const ParticlesValue&, const ParticlesValue&) = default;
    };

    using Value = std::variant<ByteValue, IntValue, FloatValue, TextValue, OptionalTextValue, ItemValue, BoolValue,
                               RotationsValue, VillagerDataValue, OptionalBlockPosValue, PoseValue, RegistryIdValue,
                               ParticleValue, ParticlesValue>;

    /// Which option shape a particle type carries; nothing for a type with none.
    using ParticleShapeOf = std::function<std::optional<ParticleOptionShape>(std::int32_t particleId)>;

    struct Entry {
        std::uint8_t index = 0;
        Value value = ByteValue{};

        friend bool operator==(const Entry&, const Entry&) = default;
    };

    std::int32_t entityId = 0;
    std::vector<Entry> entries;

    void write(io::ByteBuffer& buffer) const;
    /// A particle entry can only be read with `shapeOf`, since the wire does not say how long its
    /// options are; without one it is refused.
    [[nodiscard]] static std::optional<SetEntityDataPacket> read(io::ByteBuffer& buffer,
                                                                 const ParticleShapeOf& shapeOf = {});

    /// The one entry a dropped item needs.
    [[nodiscard]] static SetEntityDataPacket item(std::int32_t entityId, std::optional<WireItemStack> stack);

    /// The item an item frame displays.
    [[nodiscard]] static SetEntityDataPacket itemFrame(std::int32_t entityId, std::optional<WireItemStack> stack);

    /// The air left and the shared flags, which is what the HUD draws and what everyone else sees:
    /// burning, crouching, sprinting.
    ///
    /// The flags are one byte for all of them, so a caller that knows about only one erases the
    /// rest. That is exactly what used to happen to crouching: it was written by the drowning tick,
    /// which knew about fire and nothing else.
    [[nodiscard]] static SetEntityDataPacket submersion(std::int32_t entityId, std::int32_t airSupply,
                                                        std::uint8_t flags);

    /// The flags and the pose together: how an entity is standing, for everyone watching.
    [[nodiscard]] static SetEntityDataPacket posture(std::int32_t entityId, std::uint8_t flags, Pose pose);

    /// Which skin layers a player has switched on, straight from their own client settings.
    [[nodiscard]] static SetEntityDataPacket skinParts(std::int32_t entityId, std::int8_t skinParts);

    /// Lying in a bed, or standing again when there is none.
    [[nodiscard]] static SetEntityDataPacket sleeping(std::int32_t entityId, std::optional<BlockPos> bed);

    /// The yellow hearts on top of a player's own health bar.
    [[nodiscard]] static SetEntityDataPacket playerAbsorption(std::int32_t entityId, float amount);

    friend bool operator==(const SetEntityDataPacket&, const SetEntityDataPacket&) = default;
};

} // namespace basalt::wire::protocol::play
