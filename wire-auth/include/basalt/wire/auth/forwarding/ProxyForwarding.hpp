#pragma once

#include <basalt/wire/auth/SessionProfile.hpp>
#include <basalt/wire/auth/forwarding/BasaltForwarding.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::auth::forwarding {

enum class ForwardingMode : std::uint8_t {
    None,
    BungeeCord,
    BungeeGuard,
    SafeNet,
    Velocity,
    Basalt,
};

[[nodiscard]] std::string_view forwardingModeName(ForwardingMode mode) noexcept;

[[nodiscard]] std::optional<ForwardingMode> parseForwardingMode(std::string_view name);

[[nodiscard]] constexpr bool isLegacyForwarding(ForwardingMode mode) noexcept {
    return mode == ForwardingMode::BungeeCord || mode == ForwardingMode::BungeeGuard || mode == ForwardingMode::SafeNet;
}

struct ForwardedLogin {
    std::string host;
    std::string address;
    common::Uuid profileId;
    std::optional<std::string> name;
    std::vector<SessionProfileProperty> properties;

    friend bool operator==(const ForwardedLogin&, const ForwardedLogin&) = default;
};

/// A rejection's reason is for the log only: showing it would tell the player why the guess was wrong.
using ForwardingResult = std::variant<ForwardedLogin, std::string>;

[[nodiscard]] inline bool accepted(const ForwardingResult& result) noexcept {
    return std::holds_alternative<ForwardedLogin>(result);
}

struct ProxyForwarding {
    static constexpr std::string_view kBungeeGuardProperty = "bungeeguard-token";
    static constexpr std::string_view kSafeNetProperty = "safe_net_passphrase";

    ForwardingMode mode = ForwardingMode::None;
    std::string secret;
    std::vector<std::string> tokens;
    std::string propertyName{kSafeNetProperty};
    std::vector<native::PublicKey> trustedKeys;

    [[nodiscard]] bool enabled() const noexcept { return mode != ForwardingMode::None; }

    [[nodiscard]] bool authenticated() const noexcept;

    [[nodiscard]] std::optional<std::string> misconfiguration() const;

    [[nodiscard]] std::string_view tokenProperty() const noexcept;
};

} // namespace basalt::wire::auth::forwarding
