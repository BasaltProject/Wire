#pragma once

#include <basalt/wire/auth/SessionProfile.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::auth::forwarding {

/// How a proxy tells this server who is really connecting. Every mode replaces the session
/// exchange, and what they differ in is what stops anybody else from claiming to be the proxy.
enum class ForwardingMode : std::uint8_t {
    /// Nobody is forwarding. The server authenticates its own players.
    None,
    /// BungeeCord's `ip_forward`: the data is appended to the handshake host, unsigned. Nothing
    /// proves it came from the proxy, so the port has to be unreachable from outside.
    BungeeCord,
    /// BungeeCord forwarding plus a shared token in the profile properties, as BungeeGuard injects.
    BungeeGuard,
    /// The same idea with SafeNET's own property name and passphrase.
    SafeNet,
    /// Velocity's modern forwarding: an HMAC-SHA256 signed payload, asked for over a login query.
    Velocity,
};

/// What an operator writes in the configuration file.
[[nodiscard]] std::string_view forwardingModeName(ForwardingMode mode) noexcept;

/// Accepts the names other software uses for the same thing, so a copied configuration line works.
[[nodiscard]] std::optional<ForwardingMode> parseForwardingMode(std::string_view name);

/// Whether this mode reads its data off the handshake host rather than a login query.
[[nodiscard]] constexpr bool isLegacyForwarding(ForwardingMode mode) noexcept {
    return mode == ForwardingMode::BungeeCord || mode == ForwardingMode::BungeeGuard || mode == ForwardingMode::SafeNet;
}

/// Who a proxy says is connecting. The name is absent for the legacy modes, which never carry one.
struct ForwardedLogin {
    /// The host the player really typed, with the forwarded data stripped back off it.
    std::string host;
    /// The player's own address, which is what bans, logs and plugins should see.
    std::string address;
    common::Uuid profileId;
    std::optional<std::string> name;
    std::vector<SessionProfileProperty> properties;

    friend bool operator==(const ForwardedLogin&, const ForwardedLogin&) = default;
};

/// What came of reading a proxy's forwarded data. A rejection's reason is for the log: it is never
/// shown to the player, because it would say why the guess was wrong.
using ForwardingResult = std::variant<ForwardedLogin, std::string>;

[[nodiscard]] inline bool accepted(const ForwardingResult& result) noexcept {
    return std::holds_alternative<ForwardedLogin>(result);
}

/// The forwarding an operator configured: which mode, and the secret that mode needs.
struct ProxyForwarding {
    /// parity of name with BungeeGuard's own `bungeeguard-token`.
    static constexpr std::string_view kBungeeGuardProperty = "bungeeguard-token";
    /// SafeNET's default `property-name.handshake`.
    static constexpr std::string_view kSafeNetProperty = "safe_net_passphrase";

    ForwardingMode mode = ForwardingMode::None;
    /// Velocity's shared HMAC secret. Unused by every other mode.
    std::string secret;
    /// The tokens BungeeGuard or SafeNET will accept. More than one so keys can be rotated.
    std::vector<std::string> tokens;
    /// The property SafeNET carries its passphrase in. Configurable there, so configurable here.
    std::string propertyName{kSafeNetProperty};

    [[nodiscard]] bool enabled() const noexcept { return mode != ForwardingMode::None; }

    /// Whether this configuration proves the data came from the proxy, or merely trusts it.
    [[nodiscard]] bool authenticated() const noexcept;

    /// Why this configuration cannot be used, or nothing when it can.
    [[nodiscard]] std::optional<std::string> misconfiguration() const;

    /// The name of the property the token travels in, for the mode that uses one.
    [[nodiscard]] std::string_view tokenProperty() const noexcept;
};

} // namespace basalt::wire::auth::forwarding
