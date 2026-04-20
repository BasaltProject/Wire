#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::multiversion::compactBitStorage {

/// The way values were packed into longs before 1.16: end to end, spilling across the boundary.

[[nodiscard]] constexpr std::size_t longsFor(std::size_t size, int bits) noexcept {
    return (size * static_cast<std::size_t>(bits) + 63) / 64;
}

[[nodiscard]] std::vector<std::int32_t> unpack(std::span<const std::int64_t> words, int bits, std::size_t size);

[[nodiscard]] std::vector<std::int64_t> pack(std::span<const std::int32_t> values, int bits);

} // namespace basalt::wire::multiversion::compactBitStorage
