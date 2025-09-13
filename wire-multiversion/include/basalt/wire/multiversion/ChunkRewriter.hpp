#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/multiversion/IdMap.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::multiversion {

/// Renumbers the block states inside a chunk's section data. The section stream carries no count of
/// its own, so this reads until the bytes run out rather than needing to know how tall the world is.
///
/// Biomes are copied byte for byte: their ids are handed out in the order the server sends the
/// registry, so both sides already agree on them.
class ChunkRewriter {
public:
    struct Options {
        /// How wide a direct palette is on the older side: `ceil(log2(states))` of that version.
        int olderDirectBits = 15;
        /// What to show where the older version has nothing at all, normally its air.
        std::int32_t fallbackState = 0;
        /// Whether the older side counts its packed longs out loud.
        ///
        /// Up to 1.21.4 a palette wrote the length of its data array and then the array; from 1.21.5
        /// the length is left out, because both sides can work it out from the width and the
        /// container size. A client that expects the number and does not get it reads the next field
        /// as a length.
        bool lengthPrefixedData = false;
        /// Whether the far end counts the fluids in a section.
        ///
        /// 26.1 added a second short beside the block count. Everything below reads one short and
        /// then the palette, so leaving it in shifts every byte of every chunk they are sent.
        bool fluidCount = true;
        /// Whether the sections coming in still carry it. A link below the one that took it out
        /// reads what that link wrote, not what this server does.
        bool readsFluidCount = true;
    };

    ChunkRewriter(const IdLookup& blockStates, Options options) : blockStates_(&blockStates), options_(options) {}

    [[nodiscard]] std::vector<std::uint8_t> rewrite(std::span<const std::uint8_t> sections);

    /// Just the biome containers of one column, as `chunks_biomes` carries them.
    ///
    /// Nothing is renumbered, but the container itself may need writing the older way, which is the
    /// whole reason this exists rather than the bytes being passed through.
    [[nodiscard]] std::vector<std::uint8_t> rewriteBiomes(std::span<const std::uint8_t> biomes);

    /// Copies a container across, adding the length the older side expects when it needs one.
    void copyContainer(io::ByteBuffer& input, io::ByteBuffer& output, std::size_t size, int maxIndirectBits) const;

private:
    static constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
    static constexpr std::size_t kBiomesPerSection = 4 * 4 * 4;

    void writeContainer(io::ByteBuffer& output, std::span<const std::int32_t> values, int minIndirectBits,
                        int maxIndirectBits, int directBits) const;

    const IdLookup* blockStates_;
    Options options_;
    /// Reused across every section of every chunk: this runs 24 times per column sent.
    std::array<std::int32_t, kBlocksPerSection> scratch_{};
};

} // namespace basalt::wire::multiversion
