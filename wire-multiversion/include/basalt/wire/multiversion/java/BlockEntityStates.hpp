#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion::java {

/// Before 1.13 a head's kind and facing and a banner's colour lived in the block entity, not the block state.
class BlockEntityStates {
public:
    /// Keeps the last heads and banners placed, by packed position; anything else placed there forgets it.
    void remember(std::int64_t packedPos, std::int32_t nativeState);

    void remember(std::int32_t sectionX, std::int32_t sectionY, std::int32_t sectionZ, std::int16_t local,
                  std::int32_t nativeState);

    [[nodiscard]] std::optional<std::int32_t> at(std::int64_t packedPos) const;

    /// A chunk's blocks as this server sent them, read on the first block entity that needs its state.
    class Chunk {
    public:
        Chunk(std::span<const std::uint8_t> sections, bool countsFluids)
            : sections_(sections), countsFluids_(countsFluids) {}

        [[nodiscard]] std::optional<std::int32_t> at(std::uint8_t packedXZ, std::int32_t y) const;

    private:
        void read() const;

        std::span<const std::uint8_t> sections_;
        bool countsFluids_;
        mutable bool read_ = false;
        mutable std::vector<std::vector<std::int32_t>> blocks_;
        mutable std::int32_t minY_ = 0;
    };

private:
    static constexpr std::size_t kCapacity = 256;
    static constexpr std::int32_t kForgotten = -1;

    std::array<std::pair<std::int64_t, std::int32_t>, kCapacity> entries_{};
    std::size_t used_ = 0;
    std::size_t next_ = 0;
};

} // namespace basalt::wire::multiversion::java
