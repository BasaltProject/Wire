#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::common::base64 {

/// Standard alphabet with padding, which is what the status favicon data URL and Mojang's session
/// server both use.
[[nodiscard]] std::string encode(std::span<const std::uint8_t> bytes);

[[nodiscard]] std::optional<std::vector<std::uint8_t>> decode(std::string_view text);

} // namespace basalt::wire::common::base64
