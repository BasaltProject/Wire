#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// The play packets whose whole body is a handful of scalars. One header, because a file per
/// two-field packet is harder to find things in than a file per subject.
namespace basalt::wire::protocol::play {

/// Clientbound, id 0x16: an item that cannot be used again yet. The group is usually the item's own
/// name; what it does on the client is grey the slot out and sweep the little clock over it.
struct CooldownPacket {
    static constexpr std::int32_t kId = 0x16;

    std::string cooldownGroup;
    std::int32_t durationTicks = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<CooldownPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const CooldownPacket&, const CooldownPacket&) = default;
};

/// A message on a named channel, in either direction.
///
/// The body has no length prefix of its own: whatever is left in the frame after the channel name is
/// the payload. That is also what makes it usable for anything, since the server is not the one
/// deciding what the bytes mean.
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

/// One counter: which kind of statistic it is, what it counts, and how far it has counted.
///
/// parity: Stat, whose codec is the stat type's registry id followed by the id of whatever that type
/// counts, in that type's own registry: a block for `mined`, an item for `used`.
struct StatCounter {
    std::int32_t statType = 0;
    std::int32_t value = 0;
    std::int32_t count = 0;

    friend bool operator==(const StatCounter&, const StatCounter&) = default;
};

/// Clientbound, id 0x03: the statistics screen's contents. The screen asks for them and shows a
/// spinner until this arrives, so a server that never answers leaves it spinning for ever.
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

/// One column's biome data: its position, and every section's biome container back to back.
struct ChunkBiomeData {
    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
    std::vector<std::uint8_t> biomes;

    /// parity: ChunkPos.asLong, x in the low half and z in the high one.
    [[nodiscard]] static constexpr std::int64_t packChunkPos(std::int32_t x, std::int32_t z) noexcept {
        return static_cast<std::int64_t>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(x))) |
                                         (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z)) << 32));
    }

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static ChunkBiomeData read(io::ByteBuffer& buffer);

    friend bool operator==(const ChunkBiomeData&, const ChunkBiomeData&) = default;
};

/// Clientbound, id 0x0d: new biomes for columns the client already has.
///
/// The point of it is that it is not a chunk resend: the blocks, block entities and light the client
/// holds stay where they are, and only the biome containers are replaced, which is what makes a
/// biome change visible without the column flickering out and back.
struct ChunksBiomesPacket {
    static constexpr std::int32_t kId = 0x0d;

    std::vector<ChunkBiomeData> chunks;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChunksBiomesPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChunksBiomesPacket&, const ChunksBiomesPacket&) = default;
};

/// One particle kind thrown out of an explosion, and how big and fast it goes. parity:
/// ExplosionParticleInfo, carried in a weighted list so an explosion can throw a mix.
struct ExplosionParticle {
    std::int32_t particleId = 0;
    float scaling = 1.0F;
    float speed = 0.0F;
    std::int32_t weight = 1;

    friend bool operator==(const ExplosionParticle&, const ExplosionParticle&) = default;
};

/// Clientbound, id 0x24: an explosion happened here.
///
/// The blocks it broke are not in this packet: those go out as ordinary block updates. What this
/// carries is everything the client cannot work out for itself, which is the whole reason an
/// explosion looks and sounds like one: the flash, the smoke, the bang, and how hard whoever was
/// standing nearby got pushed.
struct ExplodePacket {
    static constexpr std::int32_t kId = 0x24;
    /// parity: Level.DEFAULT_EXPLOSION_BLOCK_PARTICLES, a poof and some smoke, evenly weighted.
    static constexpr float kPoofScaling = 0.5F;
    static constexpr float kSmokeScaling = 1.0F;

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float radius = 0.0F;
    std::int32_t blockCount = 0;
    /// Absent when the explosion pushed nobody, which is the common case.
    std::optional<double> knockbackX;
    double knockbackY = 0.0;
    double knockbackZ = 0.0;
    std::int32_t particleId = 0;
    std::string sound;
    std::vector<ExplosionParticle> blockParticles;

    void write(io::ByteBuffer& buffer) const;
    /// Only the sound holder write emits is understood: a direct name, never a registry id.
    [[nodiscard]] static std::optional<ExplodePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ExplodePacket&, const ExplodePacket&) = default;
};

} // namespace basalt::wire::protocol::play
