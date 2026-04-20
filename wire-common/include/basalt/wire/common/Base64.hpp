#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::common::base64 {

[[nodiscard]] std::string encode(std::span<const std::uint8_t> bytes);

[[nodiscard]] std::optional<std::vector<std::uint8_t>> decode(std::string_view text);

} // namespace basalt::wire::common::base64

namespace basalt::wire::common::base64url {

[[nodiscard]] std::string encode(std::span<const std::uint8_t> bytes);

/// Takes either alphabet, with or without padding, because a client's chain mixes the two.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> decode(std::string_view text);

} // namespace basalt::wire::common::base64url
