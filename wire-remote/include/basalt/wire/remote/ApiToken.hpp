#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace basalt::wire::remote {

inline constexpr std::string_view kDefaultApiTokenPath = ".basalt/api.token";

struct StoredToken {
    std::string token;
    bool created = false;
};

/// The token is state, not configuration: 64 hex characters generated once and kept owner-only.
[[nodiscard]] std::expected<StoredToken, std::string> loadOrCreateToken(const std::string& path);

} // namespace basalt::wire::remote
