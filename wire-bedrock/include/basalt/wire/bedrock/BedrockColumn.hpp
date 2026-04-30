#pragma once

#include <basalt/wire/bedrock/BedrockBiomes.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace basalt::wire::bedrock {

struct ColumnShape {
    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
    /// Bedrock's index of the lowest section: four below zero for the overworld.
    std::int32_t minSection = -4;
    /// The column's height in sections, which is how many biome sections it has.
    std::int32_t sections = 24;
    /// How many sections from the bottom are written as subchunks; the empty ones above are left out.
    std::int32_t sent = 0;
    std::int32_t dimension = 0;
    int protocol = 0;
};

/// Fills the 4096 Java block states of one section, in the order `writeSubChunk` takes.
using FillBlocks = std::function<void(std::int32_t section, std::span<std::int32_t> blocks)>;

/// Fills one section's biomes as Bedrock ids.
using FillBiomes = std::function<void(std::int32_t section, BiomeQuarts& quarts)>;

/// Writes the block entity tags of a column, asked for once its blocks have been read so they can be noted on the way.
using FillBlockEntities = std::function<void(io::ByteBuffer& tags)>;

/// One `LevelChunk` packet body for a column; the caller says where the blocks and biomes come from.
[[nodiscard]] std::vector<std::uint8_t> encodeColumn(const ColumnShape& shape, const FillBlocks& blocks,
                                                     const FillBiomes& biomes,
                                                     const FillBlockEntities& blockEntities = {});

} // namespace basalt::wire::bedrock
