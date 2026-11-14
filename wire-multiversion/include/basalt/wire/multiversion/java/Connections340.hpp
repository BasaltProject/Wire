#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace basalt::wire::multiversion::java::connections340 {

/// The 1.12.2 blocks a client has been sent, `id << 4 | meta`, kept to work out what 1.13 joins up.
class LegacyWorld {
public:
    static constexpr int kSections = 16;
    static constexpr std::size_t kSectionBlocks = 16 * 16 * 16;

    using Section = std::array<std::uint16_t, kSectionBlocks>;

    [[nodiscard]] std::int32_t at(std::int32_t x, std::int32_t y, std::int32_t z) const;

    void set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t legacy);

    /// Replaces one section; null clears it to air.
    void setSection(std::int32_t chunkX, std::int32_t chunkZ, int section, std::unique_ptr<Section> blocks);

    [[nodiscard]] bool loaded(std::int32_t chunkX, std::int32_t chunkZ) const;

    void load(std::int32_t chunkX, std::int32_t chunkZ);

    void unload(std::int32_t chunkX, std::int32_t chunkZ);

    void clear() { columns_.clear(); }

private:
    using Column = std::array<std::unique_ptr<Section>, kSections>;

    std::unordered_map<std::int64_t, Column> columns_;
};

/// The 1.13 state at a place whose unjoined state is `state`, given what is around it.
[[nodiscard]] std::int32_t connect(const LegacyWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                   std::int32_t state);

/// A note block tuned to `note` on `instrument`, numbered as 1.12.2 numbers its block event.
[[nodiscard]] std::int32_t noteBlock(std::int32_t instrument, std::int32_t note);

/// Whether the look of a block of this 1.13 state depends on its neighbours.
[[nodiscard]] bool joins(std::int32_t state);

/// The same, only for what depends on the blocks beside it rather than above or below.
[[nodiscard]] bool joinsSideways(std::int32_t state);

} // namespace basalt::wire::multiversion::java::connections340
