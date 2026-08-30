#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::wire::common {

/// FIPS 180-4 SHA-256.
///
/// Here rather than behind mbedTLS because this module has no dependencies and one caller is the
/// biome zoom seed, which every world needs before anything is loaded. `Md5` next to it is here for
/// the same reason.
[[nodiscard]] std::array<std::uint8_t, 32> sha256(std::span<const std::uint8_t> input);

[[nodiscard]] std::array<std::uint8_t, 32> sha256(std::string_view input);

} // namespace basalt::wire::common
