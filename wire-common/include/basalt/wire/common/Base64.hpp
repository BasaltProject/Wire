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

/// The URL-safe alphabet without padding, which is what a JWT is written in: its three parts are
/// separated by dots, and a '+' or a '/' would not survive the trip.
namespace basalt::wire::common::base64url {

[[nodiscard]] std::string encode(std::span<const std::uint8_t> bytes);

/// Takes either alphabet, with or without padding, because a client's chain mixes the two.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> decode(std::string_view text);

} // namespace basalt::wire::common::base64url
