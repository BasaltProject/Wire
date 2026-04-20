#pragma once

#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/auth/SessionProfile.hpp>

#include <array>
#include <chrono>
#include <expected>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace basalt::wire::auth {

class SessionKeyPair;
using LoginSecret = std::array<std::uint8_t, 16>;

struct OnlineLoginRequest {
    std::string name;
    std::string address;
    std::array<std::uint8_t, 4> challenge;
    std::vector<std::uint8_t> encryptedSecret;
    std::vector<std::uint8_t> encryptedChallenge;
    std::chrono::steady_clock::time_point deadline;
};

enum class LoginAuthenticationError {
    InvalidRequest,
    InvalidKey,
    InvalidChallenge,
    Rejected,
    Unavailable,
    TimedOut,
    Canceled
};

struct LoginAuthenticationFailure {
    LoginAuthenticationError kind;
    /// Present only after valid key exchange. An HTTP rejection must be sent using this cipher.
    std::optional<LoginSecret> secret;
};

struct AuthenticatedLogin {
    LoginSecret secret;
    SessionProfile profile;
};

using AuthenticationFetch = std::function<std::expected<HttpsResponse, HttpsError>(
    std::string_view, std::chrono::steady_clock::time_point, std::stop_token)>;

[[nodiscard]] bool validLoginName(std::string_view name) noexcept;

/// Worker only; never falls back to an offline identity, and an injected fetch must be thread-safe.
[[nodiscard]] std::expected<AuthenticatedLogin, LoginAuthenticationFailure>
authenticateLogin(const SessionKeyPair& key, const OnlineLoginRequest& request, std::stop_token stop,
                  const AuthenticationFetch& fetch = {});

} // namespace basalt::wire::auth
