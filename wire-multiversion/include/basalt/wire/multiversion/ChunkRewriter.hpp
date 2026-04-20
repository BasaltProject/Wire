#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/multiversion/IdMap.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::multiversion {

class ChunkRewriter {
public:
    struct Options {
        int olderDirectBits = 15;
        std::int32_t fallbackState = 0;
        /// A client that expects the number and does not get it reads the next field as a length.
        bool lengthPrefixedData = false;
        bool fluidCount = true;
        /// A link below the one that took it out reads what that link wrote, not what this server does.
        bool readsFluidCount = true;
    };

    ChunkRewriter(const IdLookup& blockStates, Options options) : blockStates_(&blockStates), options_(options) {}

    [[nodiscard]] std::vector<std::uint8_t> rewrite(std::span<const std::uint8_t> sections);

    [[nodiscard]] std::vector<std::uint8_t> rewriteBiomes(std::span<const std::uint8_t> biomes);

    void copyContainer(io::ByteBuffer& input, io::ByteBuffer& output, std::size_t size, int maxIndirectBits) const;

private:
    static constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
    static constexpr std::size_t kBiomesPerSection = 4 * 4 * 4;

    void writeContainer(io::ByteBuffer& output, std::span<const std::int32_t> values, int minIndirectBits,
                        int maxIndirectBits, int directBits) const;

    const IdLookup* blockStates_;
    Options options_;
    std::array<std::int32_t, kBlocksPerSection> scratch_{};
};

} // namespace basalt::wire::multiversion
