#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::io {
class TcpAddress;
}

namespace basalt::wire::auth {

struct HttpsResponse {
    std::int32_t status = 0;
    std::vector<std::uint8_t> body;

    [[nodiscard]] bool ok() const noexcept { return status >= 200 && status < 300; }

    [[nodiscard]] std::string text() const { return std::string{body.begin(), body.end()}; }
};

struct HttpsError {
    enum class Kind { Transport, Tls, InvalidRequest, TimedOut, Canceled, Limit, MalformedResponse };
    std::string message;
    Kind kind = Kind::Transport;
};

struct HttpsOptions {
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::stop_token stop;
    /// Required for a deadline/cancelable request so synchronous system DNS cannot escape its budget.
    std::span<const io::TcpAddress> resolvedAddresses;
    /// An explicit private CA bundle; empty uses system trust. Certificate verification is never disabled.
    std::string_view trustPem;
};

struct BoundedHttpsOptions {
    std::chrono::steady_clock::time_point deadline;
    std::stop_token stop;
    std::string_view trustPem;
    std::string_view dnsServers;
};

inline constexpr std::size_t kDefaultMaxBytes = 512UL * 1024UL * 1024UL;

using HttpsProgress = std::function<void(std::size_t bytesSoFar)>;

[[nodiscard]] std::expected<HttpsResponse, HttpsError> httpsGet(std::string_view url,
                                                                std::size_t maxBytes = kDefaultMaxBytes,
                                                                const HttpsProgress& onProgress = {},
                                                                const HttpsOptions& options = {});

/// Call on a bounded worker: local trust and resolver configuration reads are blocking file operations.
[[nodiscard]] std::expected<HttpsResponse, HttpsError> httpsGetBounded(std::string_view url, std::size_t maxBytes,
                                                                       const BoundedHttpsOptions& options,
                                                                       const HttpsProgress& onProgress = {});

} // namespace basalt::wire::auth
