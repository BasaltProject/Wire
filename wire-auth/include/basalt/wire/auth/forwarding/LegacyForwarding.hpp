#pragma once

#include <basalt/wire/auth/forwarding/ProxyForwarding.hpp>

#include <string>
#include <string_view>

namespace basalt::wire::auth::forwarding::legacy {

/// BungeeCord's IP forwarding, and the two schemes that bolt a shared secret onto it: the handshake
/// host carries the real host, the address, an undashed uuid and a JSON property array, separated
/// by NUL bytes.
///
/// parity: net.md_5.bungee.connection.InitialHandler, BungeeGuard's BungeeCordHandshake and
/// SafeNET's Authenticator.

/// Reads the host, checking the token the configuration asks for. The token is stripped out of the
/// properties that come back: it is a password, not part of the profile.
[[nodiscard]] ForwardingResult decode(std::string_view host, const ProxyForwarding& forwarding);

/// The host a proxy puts on the wire, the writing half of decode. The server never calls it.
[[nodiscard]] std::string encode(std::string_view host, std::string_view address, const common::Uuid& profileId,
                                 const std::vector<SessionProfileProperty>& properties,
                                 std::string_view tokenProperty = {}, std::string_view token = {});

} // namespace basalt::wire::auth::forwarding::legacy
