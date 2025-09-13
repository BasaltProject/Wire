#pragma once

#include <basalt/wire/auth/forwarding/ProxyForwarding.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::wire::auth::forwarding::velocity {

/// Velocity's modern forwarding: an HMAC-SHA256 tag over a version, the player's address, uuid, name
/// and properties. Versions 2 and 3 append a chat signing key, whose bytes are ignored rather than
/// refused.

/// parity of name with Velocity's PlayerDataForwarding.CHANNEL.
inline constexpr std::string_view kChannel = "velocity:player_info";

inline constexpr std::int32_t kVersionDefault = 1;
inline constexpr std::int32_t kVersionWithKey = 2;
inline constexpr std::int32_t kVersionWithKeyV2 = 3;
inline constexpr std::int32_t kVersionLazySession = 4;

/// The newest forwarding this server understands, which is what the query asks for.
inline constexpr std::int32_t kMaxSupportedVersion = kVersionLazySession;

/// What goes in the login query: one byte naming the newest version this server can read.
[[nodiscard]] std::vector<std::uint8_t> request();

/// Reads a proxy's answer, signature first: parsing an unsigned payload would be answering a guess.
[[nodiscard]] ForwardingResult decode(std::span<const std::uint8_t> data, const ProxyForwarding& forwarding);

/// The answer a proxy sends, signed. This is the writing half of decode: the server never calls it,
/// and the proxy in front of it is exactly what does.
[[nodiscard]] std::vector<std::uint8_t> sign(std::string_view secret, std::string_view address,
                                             const common::Uuid& profileId, std::string_view name,
                                             const std::vector<SessionProfileProperty>& properties,
                                             std::int32_t version = kMaxSupportedVersion);

} // namespace basalt::wire::auth::forwarding::velocity
