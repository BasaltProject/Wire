#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// One header, because a file per two-field packet is harder to find things in than a file per subject.
namespace basalt::wire::protocol::play {

/// Clientbound, id 0x16: an item that cannot be used again yet.
struct CooldownPacket {
    static constexpr std::int32_t kId = 0x16;

    std::string cooldownGroup;
    std::int32_t durationTicks = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<CooldownPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const CooldownPacket&, const CooldownPacket&) = default;
};

/// The body has no length prefix of its own: whatever is left in the frame after the channel name is the payload.
struct CustomPayloadPacket {
    static constexpr std::int32_t kClientboundId = 0x18;
    static constexpr std::int32_t kServerboundId = 0x16;
    /// The one vanilla sends itself, and the reason a server sees a payload before any plugin does.
    static constexpr std::string_view kBrandChannel = "minecraft:brand";

    std::string channel;
    std::vector<std::uint8_t> data;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<CustomPayloadPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const CustomPayloadPacket&, const CustomPayloadPacket&) = default;
};

/// parity: Stat.
struct StatCounter {
    std::int32_t statType = 0;
    std::int32_t value = 0;
    std::int32_t count = 0;

    friend bool operator==(const StatCounter&, const StatCounter&) = default;
};

struct AwardStatsPacket {
    static constexpr std::int32_t kId = 0x03;

    /// parity: the registration order of Stats in `minecraft:stat_type`.
    static constexpr std::int32_t kTypeMined = 0;
    static constexpr std::int32_t kTypeCrafted = 1;
    static constexpr std::int32_t kTypeUsed = 2;
    static constexpr std::int32_t kTypeBroken = 3;
    static constexpr std::int32_t kTypePickedUp = 4;
    static constexpr std::int32_t kTypeDropped = 5;
    static constexpr std::int32_t kTypeKilled = 6;
    static constexpr std::int32_t kTypeKilledBy = 7;
    static constexpr std::int32_t kTypeCustom = 8;

    std::vector<StatCounter> stats;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<AwardStatsPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const AwardStatsPacket&, const AwardStatsPacket&) = default;
};

struct ChunkBiomeData {
    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
    std::vector<std::uint8_t> biomes;

    /// parity: ChunkPos.asLong.
    [[nodiscard]] static constexpr std::int64_t packChunkPos(std::int32_t x, std::int32_t z) noexcept {
        return static_cast<std::int64_t>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(x))) |
                                         (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z)) << 32));
    }

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static ChunkBiomeData read(io::ByteBuffer& buffer);

    friend bool operator==(const ChunkBiomeData&, const ChunkBiomeData&) = default;
};

struct ChunksBiomesPacket {
    static constexpr std::int32_t kId = 0x0d;

    std::vector<ChunkBiomeData> chunks;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChunksBiomesPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChunksBiomesPacket&, const ChunksBiomesPacket&) = default;
};

struct ExplosionParticle {
    std::int32_t particleId = 0;
    float scaling = 1.0F;
    float speed = 0.0F;
    std::int32_t weight = 1;

    friend bool operator==(const ExplosionParticle&, const ExplosionParticle&) = default;
};

struct ExplodePacket {
    static constexpr std::int32_t kId = 0x24;
    /// parity: Level.DEFAULT_EXPLOSION_BLOCK_PARTICLES.
    static constexpr float kPoofScaling = 0.5F;
    static constexpr float kSmokeScaling = 1.0F;

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float radius = 0.0F;
    std::int32_t blockCount = 0;
    std::optional<double> knockbackX;
    double knockbackY = 0.0;
    double knockbackZ = 0.0;
    std::int32_t particleId = 0;
    std::string sound;
    std::vector<ExplosionParticle> blockParticles;
    bool playSound = true;

    void write(io::ByteBuffer& buffer) const;
    /// Only the sound holder write emits is understood: a direct name, never a registry id.
    [[nodiscard]] static std::optional<ExplodePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ExplodePacket&, const ExplodePacket&) = default;
};

} // namespace basalt::wire::protocol::play
