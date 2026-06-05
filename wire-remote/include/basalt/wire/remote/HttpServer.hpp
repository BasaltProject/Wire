#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/remote/Http.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace basalt::wire::remote {

struct HttpServerLimits {
    std::int32_t maxConnections = 32;
    std::int32_t maxConnectionsPerAddress = 8;
    std::int32_t maxBodyBytes = 65536;
    std::int32_t idleSeconds = 60;
    std::int32_t maxFailures = 5;
    std::int32_t lockoutSeconds = 30;
    std::int32_t requestsPerMinute = 120;
};

struct HttpReply {
    int status = 200;
    std::string body;
    std::string contentType = "application/json";
};

/// Runs on the server thread for authenticated requests only: it must return quickly and never block.
using HttpHandler = std::function<HttpReply(const http::Request& request, std::string_view peer)>;

/// Plain HTTP/1.1 on one thread with a bearer token, per-address lockout and rate limit; TLS belongs in front of it.
class HttpServer {
public:
    [[nodiscard]] static std::expected<std::unique_ptr<HttpServer>, std::string>
    start(const std::string& bind, std::uint16_t port, std::string token, HttpServerLimits limits, HttpHandler handler,
          common::Logger logger);

    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void stop();
    [[nodiscard]] std::uint16_t port() const noexcept;

private:
    struct Impl;
    explicit HttpServer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace basalt::wire::remote
