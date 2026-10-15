#pragma once

#include <basalt/wire/auth/forwarding/ProxyForwarding.hpp>

#include <string>
#include <string_view>

namespace basalt::wire::auth::forwarding::legacy {

/// parity: net.md_5.bungee.connection.InitialHandler, BungeeGuard BungeeCordHandshake, SafeNET Authenticator.
[[nodiscard]] ForwardingResult decode(std::string_view host, const ProxyForwarding& forwarding);

[[nodiscard]] std::string encode(std::string_view host, std::string_view address, const common::Uuid& profileId,
                                 const std::vector<SessionProfileProperty>& properties,
                                 std::string_view tokenProperty = {}, std::string_view token = {});

/// Never exceeds maxBytes; rejects reserved token collisions and NUL injection.
[[nodiscard]] std::optional<std::string> encodeBounded(std::string_view host, std::string_view address,
                                                       const common::Uuid& profileId,
                                                       const std::vector<SessionProfileProperty>& properties,
                                                       std::size_t maxBytes, std::string_view tokenProperty = {},
                                                       std::string_view token = {});

} // namespace basalt::wire::auth::forwarding::legacy
