#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::wire::common {

/// RFC 1321 MD5. Not cryptographically safe by modern standards, but required to reproduce
/// vanilla's `RandomSupport.seedFromHashOf`, which hashes structure and feature names through
/// Guava's MD5 to derive a 128-bit seed.
[[nodiscard]] std::array<std::uint8_t, 16> md5(std::span<const std::uint8_t> input);

[[nodiscard]] std::array<std::uint8_t, 16> md5(std::string_view input);

} // namespace basalt::wire::common
