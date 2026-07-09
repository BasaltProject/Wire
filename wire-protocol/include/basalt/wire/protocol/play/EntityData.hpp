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
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace basalt::wire::protocol::play {

/// parity: Mth.packDegrees and unpackDegrees, a rotation as a single byte, 256 steps per full turn.
[[nodiscard]] std::int8_t packDegrees(float value) noexcept;

[[nodiscard]] float unpackDegrees(std::int8_t value) noexcept;

struct SpawnEntityPacket {
    static constexpr std::int32_t kId = 0x01;
    /// The protocol id of `minecraft:item` in the real 26.3 entity type registry.
    static constexpr std::int32_t kItemEntityType = 72;
    static constexpr std::int32_t kPlayerEntityType = 159;

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
    HumanoidArm = 42,
    DyeColor = 43,
};

struct SetEntityDataPacket {
    static constexpr std::int32_t kId = 0x65;

    static constexpr std::uint8_t kItemStackIndex = 8;
    static constexpr std::uint8_t kItemFrameStackIndex = 9;
    static constexpr std::uint8_t kPlayerSkinPartsIndex = 16;
    /// parity: Player.DATA_PLAYER_ABSORPTION_ID.
    static constexpr std::uint8_t kPlayerAbsorptionIndex = 17;
    /// parity: Entity.DATA_POSE.
    static constexpr std::uint8_t kPoseIndex = 6;
    /// parity: Entity.DATA_CUSTOM_NAME.
    static constexpr std::uint8_t kCustomNameIndex = 2;
    static constexpr std::uint8_t kCustomNameVisibleIndex = 3;
    static constexpr std::uint8_t kLastSharedIndex = 7;
    /// parity: LivingEntity.SLEEPING_POS_ID.
    static constexpr std::uint8_t kSleepingPosIndex = 14;
    /// parity: LivingEntity.DATA_LIVING_ENTITY_FLAGS.
    static constexpr std::uint8_t kLivingFlagsIndex = 8;
    /// parity: Entity.DATA_SHARED_FLAGS_ID.
    static constexpr std::uint8_t kSharedFlagsIndex = 0;
    /// parity: Entity.DATA_AIR_SUPPLY_ID.
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

    struct LongValue {
        std::int64_t value = 0;
        friend bool operator==(const LongValue&, const LongValue&) = default;
    };

    struct FloatValue {
        float value = 0.0F;
        friend bool operator==(const FloatValue&, const FloatValue&) = default;
    };

    struct TextValue {
        text::Component value;
        friend bool operator==(const TextValue&, const TextValue&) = default;
    };

    /// parity: `EntityDataSerializers.STRING`.
    struct StringValue {
        std::string value;
        friend bool operator==(const StringValue&, const StringValue&) = default;
    };

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

    struct RegistryIdValue {
        std::int32_t value = 0;
        RegistryHolder registry = RegistryHolder::CatVariant;
        friend bool operator==(const RegistryIdValue&, const RegistryIdValue&) = default;
    };

    /// parity: `ParticleTypes.STREAM_CODEC`.
    struct ParticleValue {
        std::int32_t particleId = 0;
        std::optional<ParticleOptions> options;
        friend bool operator==(const ParticleValue&, const ParticleValue&) = default;
    };

    /// parity: `EntityDataSerializers.PARTICLES`.
    struct ParticlesValue {
        std::vector<ParticleValue> particles;
        friend bool operator==(const ParticlesValue&, const ParticlesValue&) = default;
    };

    /// parity: `EntityDataSerializers.OPTIONAL_UNSIGNED_INT`.
    struct OptionalUnsignedIntValue {
        std::optional<std::int32_t> value;
        friend bool operator==(const OptionalUnsignedIntValue&, const OptionalUnsignedIntValue&) = default;
    };

    /// An entry the sender encoded itself: the serializer's id and the bytes that follow it.
    struct RawValue {
        std::int32_t serializer = 0;
        std::vector<std::uint8_t> bytes;
        friend bool operator==(const RawValue&, const RawValue&) = default;
    };

    using Value = std::variant<ByteValue, IntValue, FloatValue, TextValue, OptionalTextValue, ItemValue, BoolValue,
                               RotationsValue, VillagerDataValue, OptionalBlockPosValue, PoseValue, RegistryIdValue,
                               ParticleValue, ParticlesValue, OptionalUnsignedIntValue, LongValue, StringValue,
                               RawValue>;

    using ParticleShapeOf = std::function<std::optional<ParticleOptionShape>(std::int32_t particleId)>;

    struct Entry {
        std::uint8_t index = 0;
        Value value = ByteValue{};

        friend bool operator==(const Entry&, const Entry&) = default;
    };

    std::int32_t entityId = 0;
    std::vector<Entry> entries;

    void write(io::ByteBuffer& buffer) const;
    using ForeignValue = std::function<std::optional<Value>(std::int32_t serializer, io::ByteBuffer&)>;

    [[nodiscard]] static std::optional<SetEntityDataPacket> read(io::ByteBuffer& buffer,
                                                                 const ParticleShapeOf& shapeOf = {},
                                                                 std::span<const std::int32_t> serializerToNative = {},
                                                                 const ForeignValue& foreign = {});

    [[nodiscard]] static SetEntityDataPacket item(std::int32_t entityId, std::optional<WireItemStack> stack);

    [[nodiscard]] static SetEntityDataPacket itemFrame(std::int32_t entityId, std::optional<WireItemStack> stack);

    /// The flags share one byte, so a caller that sets only one erases the rest.
    [[nodiscard]] static SetEntityDataPacket submersion(std::int32_t entityId, std::int32_t airSupply,
                                                        std::uint8_t flags);

    [[nodiscard]] static SetEntityDataPacket posture(std::int32_t entityId, std::uint8_t flags, Pose pose);

    /// Which skin layers a player has switched on, straight from their own client settings.
    [[nodiscard]] static SetEntityDataPacket skinParts(std::int32_t entityId, std::int8_t skinParts);

    [[nodiscard]] static SetEntityDataPacket sleeping(std::int32_t entityId, std::optional<BlockPos> bed);

    [[nodiscard]] static SetEntityDataPacket playerAbsorption(std::int32_t entityId, float amount);

    friend bool operator==(const SetEntityDataPacket&, const SetEntityDataPacket&) = default;
};

} // namespace basalt::wire::protocol::play
