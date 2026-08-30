#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <optional>
#include <vector>

/// The packets that describe the world itself: its chunks, its light, its clocks and the blocks
/// inside it. Split from the session packets because a chunk send and a tab list have nothing to do
/// with each other beyond travelling down the same socket.
namespace basalt::wire::protocol::play {

enum class HeightmapType : std::uint8_t {
    WorldSurfaceWg,
    WorldSurface,
    OceanFloorWg,
    OceanFloor,
    MotionBlocking,
    MotionBlockingNoLeaves,
};

struct Heightmap {
    HeightmapType type = HeightmapType::MotionBlocking;
    std::vector<std::int64_t> data;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<Heightmap> read(io::ByteBuffer& buffer);

    friend bool operator==(const Heightmap&, const Heightmap&) = default;
};

/// Clientbound, id 0x26: a miscellaneous world or game-state notification, one of a fixed set of
/// numeric event types.
///
/// The chunk-load-start event is what vanilla's join sequence sends right before chunk data, and the
/// client only starts waiting for chunks once it sees it: without it the loading screen never
/// dismisses, even with correct chunks and a correct batch wrapper. A game mode change rides on this
/// packet too; vanilla has no dedicated one.
struct GameEventPacket {
    static constexpr std::int32_t kId = 0x26;
    static constexpr std::uint8_t kStartRaining = 1;
    static constexpr std::uint8_t kStopRaining = 2;
    static constexpr std::uint8_t kChangeGameMode = 3;
    static constexpr std::uint8_t kRainLevelChange = 7;
    static constexpr std::uint8_t kThunderLevelChange = 8;
    static constexpr std::uint8_t kLevelChunksLoadStart = 13;

    std::uint8_t event = 0;
    float param = 0.0F;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<GameEventPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const GameEventPacket&, const GameEventPacket&) = default;
};

/// The network form of ClientboundLevelChunkPacketData.BlockEntityInfo.
struct ChunkBlockEntityInfo {
    std::uint8_t packedXZ = 0;
    std::int16_t y = 0;
    std::int32_t typeId = 0;
    std::optional<nbt::NbtCompound> tag;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChunkBlockEntityInfo> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChunkBlockEntityInfo&, const ChunkBlockEntityInfo&) = default;
};

/// The light half of a chunk, which travels both inside a chunk packet and on its own.
struct LightData {
    std::vector<std::int64_t> skyLightMask;
    std::vector<std::int64_t> blockLightMask;
    std::vector<std::int64_t> emptySkyLightMask;
    std::vector<std::int64_t> emptyBlockLightMask;
    std::vector<std::vector<std::uint8_t>> skyLight;
    std::vector<std::vector<std::uint8_t>> blockLight;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<LightData> read(io::ByteBuffer& buffer);

    friend bool operator==(const LightData&, const LightData&) = default;
};

/// Clientbound, id 0x2d: one chunk column's block and biome data.
struct MapChunkPacket {
    static constexpr std::int32_t kId = 0x2d;

    std::int32_t x = 0;
    std::int32_t z = 0;
    std::vector<Heightmap> heightmaps;
    std::vector<std::uint8_t> chunkData;
    std::vector<ChunkBlockEntityInfo> blockEntities;
    LightData light;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<MapChunkPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const MapChunkPacket&, const MapChunkPacket&) = default;
};

/// Clientbound, id 0x0c: marks the start of a batch of chunk sends. No payload.
inline constexpr std::int32_t kChunkBatchStartId = 0x0c;

/// Clientbound, id 0x25: discard a chunk column that fell outside the view distance. Its body is one
/// packed chunk position, not two separate ints like the chunk packet.
struct ForgetLevelChunkPacket {
    static constexpr std::int32_t kId = 0x25;

    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ForgetLevelChunkPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ForgetLevelChunkPacket&, const ForgetLevelChunkPacket&) = default;
};

/// Clientbound, id 0x0b: the end of a chunk batch, with how many chunks it held.
///
/// Without wrapping chunk sends in a start and finish pair a real client's loading screen never
/// dismisses: it tracks progress by batch, not by individual chunk arrivals.
struct ChunkBatchFinishedPacket {
    static constexpr std::int32_t kId = 0x0b;

    std::int32_t batchSize = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChunkBatchFinishedPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChunkBatchFinishedPacket&, const ChunkBatchFinishedPacket&) = default;
};

/// Serverbound, id 0x0b: the client's acknowledgement of a batch, with its desired rate.
struct ChunkBatchReceivedPacket {
    static constexpr std::int32_t kId = 0x0b;

    float desiredChunksPerTick = 0.0F;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChunkBatchReceivedPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChunkBatchReceivedPacket&, const ChunkBatchReceivedPacket&) = default;
};

/// Clientbound, id 0x30: the light half of chunk sync, sent alongside the chunk itself.
struct UpdateLightPacket {
    static constexpr std::int32_t kId = 0x30;

    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
    LightData light;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<UpdateLightPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const UpdateLightPacket&, const UpdateLightPacket&) = default;
};

/// Clientbound, id 0x5e: which chunk is the centre of the client's loaded area. Sent on join before
/// any chunk data: without it the loading screen never dismisses, because the client never learns
/// which chunks to expect.
struct SetChunkCacheCenterPacket {
    static constexpr std::int32_t kId = 0x5e;

    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetChunkCacheCenterPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetChunkCacheCenterPacket&, const SetChunkCacheCenterPacket&) = default;
};

/// Clientbound, id 0x5f: how many chunks around the centre to expect. It has to match what the
/// server actually sends, or the client waits for chunks that never arrive.
struct SetChunkCacheRadiusPacket {
    static constexpr std::int32_t kId = 0x5f;

    std::int32_t radius = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetChunkCacheRadiusPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetChunkCacheRadiusPacket&, const SetChunkCacheRadiusPacket&) = default;
};

/// Clientbound, id 0x6f: the server's simulation distance.
struct SetSimulationDistancePacket {
    static constexpr std::int32_t kId = 0x6f;

    std::int32_t simulationDistance = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetSimulationDistancePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetSimulationDistancePacket&, const SetSimulationDistancePacket&) = default;
};

/// Clientbound, id 0x08: a single block changed.
struct ClientboundBlockUpdatePacket {
    static constexpr std::int32_t kId = 0x08;

    BlockPos pos;
    std::int32_t blockStateId = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ClientboundBlockUpdatePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ClientboundBlockUpdatePacket&, const ClientboundBlockUpdatePacket&) = default;
};

/// One clock's state: how many ticks it has counted, where it is between ticks, and how fast it
/// runs. A rate of zero is how a paused clock is expressed, since there is no paused flag.
struct ClockUpdate {
    /// The clock's index in the `minecraft:world_clock` registry: 0 the overworld, 1 the end.
    std::int32_t clockId = 0;
    std::int64_t totalTicks = 0;
    float partialTick = 0.0F;
    float rate = 1.0F;

    friend bool operator==(const ClockUpdate&, const ClockUpdate&) = default;
};

/// Clientbound, id 0x71: the world clocks' state.
///
/// 26.2 stopped sending a day-time number and sends clocks instead, each a registry entry the
/// dimension type points at. A dimension with no clock, the Nether, is simply never updated, which
/// is why its sky never moves.
struct SetTimePacket {
    static constexpr std::int32_t kId = 0x71;
    /// parity: WorldClocks, whose registration order is the id the holder carries.
    static constexpr std::int32_t kOverworldClock = 0;
    static constexpr std::int32_t kEndClock = 1;

    std::int64_t gameTime = 0;
    std::vector<ClockUpdate> clocks;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetTimePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetTimePacket&, const SetTimePacket&) = default;
};

/// Clientbound, id 0x0a: the difficulty to show, and whether the client may change it.
struct ChangeDifficultyPacket {
    static constexpr std::int32_t kId = 0x0a;

    std::uint8_t difficulty = 0;
    bool locked = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChangeDifficultyPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChangeDifficultyPacket&, const ChangeDifficultyPacket&) = default;
};

/// Clientbound, id 0x54: every block that changed inside one section, in one packet.
///
/// The section coordinate packs as SectionPos.asLong (22 bits of x, 20 of y, 22 of z), and each
/// change as a varlong of the state id shifted over a packed relative position, which is why a
/// section can only carry 4096 distinct positions.
struct SectionBlocksUpdatePacket {
    static constexpr std::int32_t kId = 0x54;

    struct Change {
        /// A section-relative position, already packed the way the wire wants it.
        std::int16_t position = 0;
        std::int32_t blockStateId = 0;

        friend bool operator==(const Change&, const Change&) = default;
    };

    std::int32_t sectionX = 0;
    std::int32_t sectionY = 0;
    std::int32_t sectionZ = 0;
    std::vector<Change> changes;

    /// parity: SectionPos.sectionRelativePos, whose order is x, then z, then y.
    [[nodiscard]] static constexpr std::int16_t packPosition(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
        return static_cast<std::int16_t>(((x & 15) << 8) | ((z & 15) << 4) | (y & 15));
    }

    [[nodiscard]] static constexpr std::int64_t packSection(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
        return static_cast<std::int64_t>(((static_cast<std::uint64_t>(x) & 0x3FFFFFULL) << 42) |
                                         ((static_cast<std::uint64_t>(z) & 0x3FFFFFULL) << 20) |
                                         (static_cast<std::uint64_t>(y) & 0xFFFFFULL));
    }

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SectionBlocksUpdatePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SectionBlocksUpdatePacket&, const SectionBlocksUpdatePacket&) = default;
};

} // namespace basalt::wire::protocol::play
