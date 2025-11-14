#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
    std::string message;
};

/// Fetches `url`, which has to be `https://`.
///
/// `maxBytes` is a ceiling on the body, since the caller knows what it asked for and a server that
/// answers with far more than that is not one to keep reading from.
[[nodiscard]] std::expected<HttpsResponse, HttpsError> httpsGet(std::string_view url,
                                                                std::size_t maxBytes = 512UL * 1024UL * 1024UL);

} // namespace basalt::wire::auth
