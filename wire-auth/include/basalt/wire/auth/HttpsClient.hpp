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

/// One HTTPS GET, and nothing else: the version manifest, a version's metadata, the two jars and the
/// session service. No redirects, cookies or request bodies, built on the mbedTLS already vendored
/// for the login handshake.
///
/// Certificates are verified against the system trust store where the platform exposes one; where it
/// does not, the request fails rather than proceeding unverified.
struct HttpsResponse {
    std::int32_t status = 0;
    std::vector<std::uint8_t> body;

    [[nodiscard]] bool ok() const noexcept { return status >= 200 && status < 300; }

    [[nodiscard]] std::string text() const { return std::string{body.begin(), body.end()}; }
};

/// Why a request could not be made at all, as opposed to being answered with an error status.
struct HttpsError {
    enum class Kind { Transport, Tls, InvalidRequest, TimedOut, Canceled, Limit, MalformedResponse };
    std::string message;
    Kind kind = Kind::Transport;
};

struct HttpsOptions {
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::stop_token stop;
    /// Required for a deadline/cancelable request so synchronous system DNS cannot escape its budget.
    /// The caller retains endpoints through the synchronous call; TLS verifies the URL host, not the IP.
    std::span<const io::TcpAddress> resolvedAddresses;
    /// An explicit private CA bundle; empty uses system trust. Certificate verification is never disabled.
    std::string_view trustPem;
};

/// The ceiling a caller gets without asking for one. Large enough for the client jar, small enough
/// that a server answering with a stream of nonsense is dropped rather than kept in memory.
inline constexpr std::size_t kDefaultMaxBytes = 512UL * 1024UL * 1024UL;

/// Told how much has arrived so far, headers included, every time more of it does. A caller that
/// knows the size it asked for turns that into a percentage; one that does not can still say that
/// something is happening.
using HttpsProgress = std::function<void(std::size_t bytesSoFar)>;

/// Fetches `url`, which has to be `https://`.
///
/// `maxBytes` is a ceiling on the body, since the caller knows what it asked for and a server that
/// answers with far more than that is not one to keep reading from. The encoded response
/// has an additional 64 KiB envelope budget for headers/chunk framing; exceeding either
/// limit is an error, not a successful partial response.
[[nodiscard]] std::expected<HttpsResponse, HttpsError> httpsGet(std::string_view url,
                                                                std::size_t maxBytes = kDefaultMaxBytes,
                                                                const HttpsProgress& onProgress = {},
                                                                const HttpsOptions& options = {});

} // namespace basalt::wire::auth
