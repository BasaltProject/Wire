#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace basalt::wire::protocol::play::nibbleArray {

/// Packs 4096 four-bit light values, one per block in a section, into 2048 bytes, low nibble first.
inline constexpr std::size_t kSectionByteCount = 4096 / 2;

void pack(std::span<const std::int32_t> values, std::span<std::uint8_t> into);

void unpack(std::span<const std::uint8_t> bytes, std::span<std::int32_t> into);

/// A section at one light level everywhere.
[[nodiscard]] std::array<std::uint8_t, kSectionByteCount> uniform(std::int32_t level);

} // namespace basalt::wire::protocol::play::nibbleArray
