#pragma once

#include <basalt/wire/auth/HttpsClient.hpp>

#include <span>

namespace basalt::wire::auth::detail {
/// Internal HTTP/1 response boundary, independently testable without TLS/network services.
[[nodiscard]] std::expected<HttpsResponse, HttpsError> decodeHttpsResponse(std::span<const std::uint8_t> raw,
                                                                           std::size_t maxBodyBytes);
} // namespace basalt::wire::auth::detail
