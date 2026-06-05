#include <basalt/wire/io/EventLoop.hpp>
#include <basalt/wire/io/TcpSocket.hpp>
#include <basalt/wire/remote/AttemptTracker.hpp>
#include <basalt/wire/remote/HttpServer.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <optional>
#include <thread>
#include <vector>

namespace basalt::wire::remote {
namespace {

using Clock = std::chrono::steady_clock;

constexpr std::uint64_t kListenerToken = 1;
constexpr std::uint64_t kFirstConnection = 16;
constexpr std::size_t kMaxOutBytes = 4 * 1024 * 1024;
constexpr std::size_t kReadChunk = 4096;
constexpr std::string_view kJson = "application/json";

struct Connection {
    io::TcpConnection socket;
    std::string peer;
    std::string in;
    std::string out;
    bool closeAfterFlush = false;
    bool writeInterest = false;
    Clock::time_point lastReceived;
};

} // namespace

struct HttpServer::Impl {
    std::string token;
    HttpServerLimits limits;
    HttpHandler handler;
    common::Logger logger;
    io::EventLoop loop;
    io::TcpListener listener;
    std::uint16_t boundPort = 0;
    AttemptTracker tracker;
    std::map<std::uint64_t, Connection> connections;
    std::uint64_t nextId = kFirstConnection;
    Clock::time_point lastSweep = Clock::now();
    std::jthread thread;
    bool running = false;

    Impl(std::string t, HttpServerLimits l, HttpHandler h, common::Logger log)
        : token(std::move(t)), limits(l), handler(std::move(h)), logger(std::move(log)),
          tracker(l.maxFailures, l.lockoutSeconds) {}

    void close(std::uint64_t id) {
        const auto found = connections.find(id);
        if (found == connections.end())
            return;
        loop.unwatch(found->second.socket.nativeHandle());
        connections.erase(found);
    }

    void accept() {
        while (true) {
            auto accepted = listener.accept();
            if (!accepted)
                return;
            static_cast<void>(accepted->setNonBlocking());
            const std::string peer = accepted->remoteAddress();
            const auto sameAddress = static_cast<std::size_t>(
                std::ranges::count_if(connections, [&peer](const auto& item) { return item.second.peer == peer; }));
            if (connections.size() >= static_cast<std::size_t>(limits.maxConnections) ||
                sameAddress >= static_cast<std::size_t>(limits.maxConnectionsPerAddress))
                continue;
            const std::uint64_t id = nextId++;
            const std::int64_t handle = accepted->nativeHandle();
            if (handle < 0 || !loop.watch(handle, {true, false}, id))
                continue;
            Connection connection;
            connection.socket = std::move(*accepted);
            connection.peer = peer;
            connection.lastReceived = Clock::now();
            connections.emplace(id, std::move(connection));
        }
    }

    void reply(Connection& connection, int status, std::string_view body, std::string_view contentType = kJson,
               bool keepAlive = true, std::string_view extra = {}) {
        const bool keep = keepAlive && status < 400 && !connection.closeAfterFlush;
        connection.out += http::response(status, contentType, body, keep, extra);
        if (!keep)
            connection.closeAfterFlush = true;
    }

    [[nodiscard]] bool bearerMatches(const http::Request& request) const {
        const auto header = request.header("authorization");
        constexpr std::string_view kBearer = "bearer ";
        if (!header || header->size() <= kBearer.size() || http::lower(header->substr(0, kBearer.size())) != kBearer)
            return false;
        return http::constantTimeEquals(header->substr(kBearer.size()), token);
    }

    void handle(Connection& connection, const http::Request& request) {
        const auto now = Clock::now();
        const bool keepAlive =
            !http::tokenListContains(http::lower(request.header("connection").value_or("")), "close");
        if (const std::int32_t locked = tracker.lockedFor(connection.peer, now); locked > 0) {
            reply(connection, 429, http::errorBody("too many failed attempts"), kJson, false,
                  "Retry-After: " + std::to_string(locked) + "\r\n");
            return;
        }
        if (!tracker.allowRequest(connection.peer, now, limits.requestsPerMinute)) {
            reply(connection, 429, http::errorBody("rate limit exceeded"), kJson, false, "Retry-After: 60\r\n");
            return;
        }
        if (!bearerMatches(request)) {
            tracker.recordFailure(connection.peer, now);
            logger.warn(
                [&connection] { return "Rejected an API request with a bad token from " + connection.peer + "."; });
            reply(connection, 401, http::errorBody("missing or invalid bearer token"), kJson, false,
                  "WWW-Authenticate: Bearer\r\n");
            return;
        }
        tracker.recordSuccess(connection.peer);
        const HttpReply result = handler(request, connection.peer);
        reply(connection, result.status, result.body, result.contentType, keepAlive);
    }

    void process(Connection& connection) {
        http::Request request;
        while (!connection.closeAfterFlush) {
            switch (http::take(connection.in, request, static_cast<std::size_t>(limits.maxBodyBytes))) {
            case http::ParseResult::NeedMore:
                return;
            case http::ParseResult::Request:
                handle(connection, request);
                continue;
            case http::ParseResult::HeadTooLarge:
                reply(connection, 431, http::errorBody("request head too large"), kJson, false);
                return;
            case http::ParseResult::BodyTooLarge:
                reply(connection, 413, http::errorBody("request body too large"), kJson, false);
                return;
            case http::ParseResult::Unsupported:
                reply(connection, 501, http::errorBody("chunked request bodies are not supported"), kJson, false);
                return;
            case http::ParseResult::Malformed:
                reply(connection, 400, http::errorBody("malformed request"), kJson, false);
                return;
            }
        }
    }

    // Returns false when the connection is gone.
    bool pump(std::uint64_t id, bool readable) {
        Connection& connection = connections.at(id);
        bool peerClosed = false;
        if (readable && !connection.closeAfterFlush) {
            std::array<std::uint8_t, kReadChunk> chunk{};
            const std::size_t cap = static_cast<std::size_t>(limits.maxBodyBytes) + http::kMaxHeadBytes + kReadChunk;
            while (connection.in.size() < cap) {
                const auto count = connection.socket.read(chunk);
                if (!count) {
                    peerClosed = count.error().kind != io::SocketError::Kind::WouldBlock;
                    break;
                }
                if (*count == 0) {
                    peerClosed = true;
                    break;
                }
                connection.in.append(reinterpret_cast<const char*>(chunk.data()), *count);
                connection.lastReceived = Clock::now();
            }
            process(connection);
        }
        while (!connection.out.empty()) {
            const auto sent = connection.socket.writeSome(
                {reinterpret_cast<const std::uint8_t*>(connection.out.data()), connection.out.size()});
            if (!sent) {
                if (sent.error().kind != io::SocketError::Kind::WouldBlock)
                    connection.out.clear(), connection.closeAfterFlush = true;
                break;
            }
            connection.out.erase(0, *sent);
        }
        if (peerClosed || (connection.closeAfterFlush && connection.out.empty()) ||
            connection.out.size() > kMaxOutBytes) {
            close(id);
            return false;
        }
        const bool wantWrite = !connection.out.empty();
        if (wantWrite != connection.writeInterest) {
            connection.writeInterest = wantWrite;
            static_cast<void>(loop.watch(connection.socket.nativeHandle(), {true, wantWrite}, id));
        }
        return true;
    }

    void sweep() {
        const auto now = Clock::now();
        std::vector<std::uint64_t> stale;
        for (const auto& [id, connection] : connections)
            if (now - connection.lastReceived > std::chrono::seconds{limits.idleSeconds})
                stale.push_back(id);
        for (const std::uint64_t id : stale)
            close(id);
        if (now - lastSweep >= std::chrono::seconds{60}) {
            lastSweep = now;
            tracker.sweep(now);
        }
    }

    void run(std::stop_token stop) {
        while (!stop.stop_requested()) {
            const auto ready = loop.wait(1000);
            if (stop.stop_requested())
                break;
            if (ready) {
                const std::vector<io::IoReady> events{ready->begin(), ready->end()};
                for (const io::IoReady& event : events) {
                    if (event.token == kListenerToken)
                        accept();
                    else if (connections.contains(event.token))
                        pump(event.token, event.readable || event.closed);
                }
            }
            sweep();
        }
        connections.clear();
    }
};

HttpServer::HttpServer(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

HttpServer::~HttpServer() { stop(); }

std::expected<std::unique_ptr<HttpServer>, std::string> HttpServer::start(const std::string& bind, std::uint16_t port,
                                                                          std::string token, HttpServerLimits limits,
                                                                          HttpHandler handler, common::Logger logger) {
    if (token.empty())
        return std::unexpected("the API token is empty");
    const auto address = io::TcpAddress::literal(bind, port);
    if (!address)
        return std::unexpected("invalid bind address " + bind);
    auto impl = std::make_unique<Impl>(std::move(token), limits, std::move(handler), std::move(logger));
    auto bound = io::TcpListener::bind(*address, 64);
    if (!bound)
        return std::unexpected("could not bind " + bind + " port " + std::to_string(port));
    static_cast<void>(bound->setNonBlocking());
    if (!impl->loop.watch(bound->nativeHandle(), {true, false}, kListenerToken))
        return std::unexpected("could not watch the listener on port " + std::to_string(port));
    const auto local = bound->localPort();
    impl->boundPort = local ? *local : port;
    impl->listener = std::move(*bound);
    Impl& raw = *impl;
    raw.running = true;
    raw.thread = std::jthread([&raw](std::stop_token stop) { raw.run(stop); });
    return std::unique_ptr<HttpServer>(new HttpServer(std::move(impl)));
}

void HttpServer::stop() {
    if (!impl_ || !impl_->running)
        return;
    impl_->running = false;
    impl_->thread.request_stop();
    static_cast<void>(impl_->loop.wake());
    impl_->thread.join();
    impl_->listener.close();
}

std::uint16_t HttpServer::port() const noexcept { return impl_->boundPort; }

} // namespace basalt::wire::remote
