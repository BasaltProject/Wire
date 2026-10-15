#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::wire::common {

[[nodiscard]] std::array<std::uint8_t, 16> md5(std::span<const std::uint8_t> input);

[[nodiscard]] std::array<std::uint8_t, 16> md5(std::string_view input);

} // namespace basalt::wire::common
