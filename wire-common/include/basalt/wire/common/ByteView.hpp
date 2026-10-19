#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::wire::common {

[[nodiscard]] inline std::string_view asText(std::span<const std::uint8_t> bytes) noexcept {
    return {std::bit_cast<const char*>(bytes.data()), bytes.size()};
}

[[nodiscard]] inline std::span<const std::uint8_t> asBytes(std::string_view text) noexcept {
    return {std::bit_cast<const std::uint8_t*>(text.data()), text.size()};
}

} // namespace basalt::wire::common
