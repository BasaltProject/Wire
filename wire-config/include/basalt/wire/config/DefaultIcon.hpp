#pragma once

#include <cstdint>
#include <span>

namespace basalt::wire::config::defaultIcon {

/// The 64x64 PNG every status answer carries when a profile names no icon of its own.
[[nodiscard]] std::span<const std::uint8_t> bytes();

} // namespace basalt::wire::config::defaultIcon
