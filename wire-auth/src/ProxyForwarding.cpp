#include <basalt/wire/auth/forwarding/ProxyForwarding.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace basalt::wire::auth::forwarding {
namespace {

constexpr std::array<std::pair<std::string_view, ForwardingMode>, 11> kNames = {{
    {"none", ForwardingMode::None},
    {"bungeecord", ForwardingMode::BungeeCord},
    {"bungeeguard", ForwardingMode::BungeeGuard},
    {"safenet", ForwardingMode::SafeNet},
    {"velocity", ForwardingMode::Velocity},
    {"basalt", ForwardingMode::Basalt},
    {"legacy", ForwardingMode::BungeeCord},
    {"bungee", ForwardingMode::BungeeCord},
    {"modern", ForwardingMode::Velocity},
    {"false", ForwardingMode::None},
    {"disabled", ForwardingMode::None},
}};

} // namespace

std::string_view forwardingModeName(ForwardingMode mode) noexcept {
    switch (mode) {
    case ForwardingMode::None:
        return "none";
    case ForwardingMode::BungeeCord:
        return "bungeecord";
    case ForwardingMode::BungeeGuard:
        return "bungeeguard";
    case ForwardingMode::SafeNet:
        return "safenet";
    case ForwardingMode::Velocity:
        return "velocity";
    case ForwardingMode::Basalt:
        return "basalt";
    }
    return "none";
}

std::optional<ForwardingMode> parseForwardingMode(std::string_view name) {
    std::string written;
    written.reserve(name.size());
    for (const char c : name) {
        if (c != ' ' && c != '\t') {
            written += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    for (const auto& [key, mode] : kNames) {
        if (key == written) {
            return mode;
        }
    }
    return std::nullopt;
}

bool ProxyForwarding::authenticated() const noexcept {
    switch (mode) {
    case ForwardingMode::None:
    case ForwardingMode::BungeeCord:
        return false;
    case ForwardingMode::BungeeGuard:
    case ForwardingMode::SafeNet:
        return !tokens.empty();
    case ForwardingMode::Velocity:
        return !secret.empty();
    case ForwardingMode::Basalt:
        return !trustedKeys.empty();
    }
    return false;
}

std::optional<std::string> ProxyForwarding::misconfiguration() const {
    switch (mode) {
    case ForwardingMode::None:
    case ForwardingMode::BungeeCord:
        return std::nullopt;
    case ForwardingMode::BungeeGuard:
        return tokens.empty() ? std::optional<std::string>{"bungeeguard forwarding needs at least one token in tokens"}
                              : std::nullopt;
    case ForwardingMode::SafeNet:
        return tokens.empty() ? std::optional<std::string>{"safenet forwarding needs at least one passphrase in tokens"}
                              : std::nullopt;
    case ForwardingMode::Velocity:
        return secret.empty() ? std::optional<std::string>{"velocity forwarding needs the proxy's forwarding.secret"}
                              : std::nullopt;
    case ForwardingMode::Basalt:
        if (trustedKeys.empty())
            return std::string{"basalt forwarding needs at least one trusted-keys entry"};
        if (backendId.empty() || backendId.size() > 64)
            return std::string{"basalt forwarding needs a backend-id of 1 to 64 bytes"};
        return std::nullopt;
    }
    return std::nullopt;
}

std::string_view ProxyForwarding::tokenProperty() const noexcept {
    return mode == ForwardingMode::SafeNet ? std::string_view{propertyName} : kBungeeGuardProperty;
}

} // namespace basalt::wire::auth::forwarding
