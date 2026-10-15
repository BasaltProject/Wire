#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/auth/test/TlsFixtures.hpp>
#include <basalt/wire/io/EventLoop.hpp>
#include <basalt/wire/io/TcpSocket.hpp>

#include <doctest.h>

#include <array>
#include <basalt_mbedtls_threads.h>
#include <chrono>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <optional>
#include <semaphore>
#include <stdexcept>
#include <string>
#include <thread>

using namespace basalt::wire;
using namespace std::chrono_literals;

namespace {
// Synthetic localhost credentials, generated for these tests only, embedded only in test targets.
struct TlsState {
    mbedtls_ssl_context ssl{};
    mbedtls_ssl_config config{};
    mbedtls_x509_crt cert{};
    mbedtls_pk_context key{};
    mbedtls_ctr_drbg_context rng{};
    mbedtls_entropy_context entropy{};

    TlsState() {
        basalt_mbedtls_threading_init();
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&config);
        mbedtls_x509_crt_init(&cert);
        mbedtls_pk_init(&key);
        mbedtls_ctr_drbg_init(&rng);
        mbedtls_entropy_init(&entropy);
    }

    ~TlsState() {
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&config);
        mbedtls_x509_crt_free(&cert);
        mbedtls_pk_free(&key);
        mbedtls_ctr_drbg_free(&rng);
        mbedtls_entropy_free(&entropy);
    }
};

class TlsServer {
public:
    explicit TlsServer(std::string response, bool stall = false, bool stallAfterResponse = false, bool ipv6 = false)
        : response_(std::move(response)), stall_(stall), stallAfterResponse_(stallAfterResponse) {
        if (ipv6) {
            literalHost_ = "::1";
        }
        const auto address = io::TcpAddress::literal(literalHost_, 0);
        if (!address) {
            throw std::runtime_error{"test address failed"};
        }
        auto listener = io::TcpListener::bind(*address);
        if (!listener) {
            throw std::runtime_error{"test listener failed"};
        }
        listener_ = std::move(*listener);
        const auto port = listener_.localPort();
        if (!port || !listener_.setNonBlocking()) {
            throw std::runtime_error{"test listener setup failed"};
        }
        port_ = *port;
        const std::string cert{auth::test::kLocalhostCertPem};
        const std::string key{auth::test::kLocalhostKeyPem};
        const std::string seed = "basalt-tls-test";
        if (mbedtls_ctr_drbg_seed(&state_.rng, mbedtls_entropy_func, &state_.entropy,
                                  reinterpret_cast<const unsigned char*>(seed.data()), seed.size()) != 0 ||
            mbedtls_x509_crt_parse(&state_.cert, reinterpret_cast<const unsigned char*>(cert.c_str()),
                                   cert.size() + 1) != 0 ||
            mbedtls_pk_parse_key(&state_.key, reinterpret_cast<const unsigned char*>(key.c_str()), key.size() + 1,
                                 nullptr, 0, mbedtls_ctr_drbg_random, &state_.rng) != 0 ||
            mbedtls_ssl_config_defaults(&state_.config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
            throw std::runtime_error{"test TLS setup failed"};
        }
        mbedtls_ssl_conf_rng(&state_.config, mbedtls_ctr_drbg_random, &state_.rng);
        if (mbedtls_ssl_conf_own_cert(&state_.config, &state_.cert, &state_.key) != 0 ||
            mbedtls_ssl_setup(&state_.ssl, &state_.config) != 0) {
            throw std::runtime_error{"test certificate setup failed"};
        }
        worker_ = std::jthread([this](std::stop_token stop) { run(stop); });
    }

    ~TlsServer() {
        worker_.request_stop();
        static_cast<void>(loop_.wake());
        worker_.join();
    }

    std::string url(std::string_view host = "localhost") const {
        return "https://" + std::string{host} + ":" + std::to_string(port_) + "/profile";
    }

    io::TcpAddress endpoint() const {
        const auto address = io::TcpAddress::literal(literalHost_, port_);
        if (!address) {
            throw std::runtime_error{"test endpoint failed"};
        }
        return *address;
    }

    bool accepted() { return accepted_.try_acquire_for(2s); }

    bool responseSent() { return sent_.try_acquire_for(2s); }

private:
    static int send(void* context, const unsigned char* bytes, std::size_t size) {
        auto& connection = *static_cast<io::TcpConnection*>(context);
        const auto sent = connection.writeSome({bytes, size});
        if (!sent) {
            return sent.error().kind == io::SocketError::Kind::WouldBlock ? MBEDTLS_ERR_SSL_WANT_WRITE
                                                                          : MBEDTLS_ERR_NET_SEND_FAILED;
        }
        return static_cast<int>(*sent);
    }

    static int receive(void* context, unsigned char* bytes, std::size_t size) {
        auto& connection = *static_cast<io::TcpConnection*>(context);
        const auto got = connection.read({bytes, size});
        if (!got) {
            return got.error().kind == io::SocketError::Kind::WouldBlock ? MBEDTLS_ERR_SSL_WANT_READ
                                                                         : MBEDTLS_ERR_NET_RECV_FAILED;
        }
        return static_cast<int>(*got);
    }

    bool wait(io::TcpConnection& peer, int wanted, std::stop_token stop) {
        if (stop.stop_requested()) {
            return false;
        }
        if (!loop_.watch(peer.nativeHandle(),
                         {wanted == MBEDTLS_ERR_SSL_WANT_READ, wanted == MBEDTLS_ERR_SSL_WANT_WRITE}, 2)) {
            return false;
        }
        const auto ready = loop_.wait(2000);
        return ready.has_value() && !stop.stop_requested();
    }

    void run(std::stop_token stop) {
        std::stop_callback onStop{stop, [this] { static_cast<void>(loop_.wake()); }};
        if (!loop_.watch(listener_.nativeHandle(), {true, false}, 1)) {
            return;
        }
        std::optional<io::TcpConnection> peer;
        while (!stop.stop_requested()) {
            const auto ready = loop_.wait(2000);
            if (!ready) {
                return;
            }
            auto accepted = listener_.accept();
            if (accepted) {
                peer.emplace(std::move(*accepted));
                break;
            }
            if (accepted.error().kind != io::SocketError::Kind::WouldBlock) {
                return;
            }
        }
        if (!peer || !peer->setNonBlocking()) {
            return;
        }
        loop_.unwatch(listener_.nativeHandle());
        accepted_.release();
        if (stall_) {
            while (!stop.stop_requested()) {
                if (!loop_.wait(2000)) {
                    return;
                }
            }
            return;
        }
        mbedtls_ssl_set_bio(&state_.ssl, &*peer, send, receive, nullptr);
        while (!stop.stop_requested()) {
            const int code = mbedtls_ssl_handshake(&state_.ssl);
            if (code == 0) {
                break;
            }
            if ((code != MBEDTLS_ERR_SSL_WANT_READ && code != MBEDTLS_ERR_SSL_WANT_WRITE) || !wait(*peer, code, stop)) {
                return;
            }
        }
        std::string request;
        std::array<unsigned char, 1024> bytes{};
        while (!stop.stop_requested() && request.find("\r\n\r\n") == std::string::npos) {
            const int got = mbedtls_ssl_read(&state_.ssl, bytes.data(), bytes.size());
            if (got == MBEDTLS_ERR_SSL_WANT_READ || got == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (!wait(*peer, got, stop)) {
                    return;
                }
            } else if (got <= 0) {
                return;
            } else {
                request.append(reinterpret_cast<const char*>(bytes.data()), static_cast<std::size_t>(got));
            }
            if (request.size() > 8192) {
                return;
            }
        }
        std::size_t sent = 0;
        while (!stop.stop_requested() && sent < response_.size()) {
            const int count = mbedtls_ssl_write(
                &state_.ssl, reinterpret_cast<const unsigned char*>(response_.data() + sent), response_.size() - sent);
            if (count == MBEDTLS_ERR_SSL_WANT_READ || count == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (!wait(*peer, count, stop)) {
                    return;
                }
            } else if (count <= 0) {
                return;
            } else {
                sent += static_cast<std::size_t>(count);
            }
        }
        sent_.release();
        if (stallAfterResponse_) {
            loop_.unwatch(peer->nativeHandle());
            while (!stop.stop_requested()) {
                if (!loop_.wait(2000)) {
                    return;
                }
            }
            return;
        }
        while (!stop.stop_requested()) {
            const int code = mbedtls_ssl_close_notify(&state_.ssl);
            if (code == 0) {
                break;
            }
            if ((code != MBEDTLS_ERR_SSL_WANT_READ && code != MBEDTLS_ERR_SSL_WANT_WRITE) || !wait(*peer, code, stop)) {
                break;
            }
        }
    }

    TlsState state_;
    io::TcpListener listener_;
    io::EventLoop loop_;
    std::uint16_t port_ = 0;
    std::string response_;
    bool stall_;
    bool stallAfterResponse_;
    std::string literalHost_ = "127.0.0.1";
    std::binary_semaphore accepted_{0};
    std::binary_semaphore sent_{0};
    std::jthread worker_;
};
} // namespace

TEST_CASE("bounded HTTPS verifies URL identity over a pre-resolved loopback endpoint") {
    TlsServer server{"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}"};
    const std::array endpoints{server.endpoint()};
    const auth::HttpsOptions options{.deadline = std::chrono::steady_clock::now() + 2s,
                                     .resolvedAddresses = endpoints,
                                     .trustPem = auth::test::kLocalhostCertPem};
    const auto response = auth::httpsGet(server.url(), 64, {}, options);
    REQUIRE(response);
    CHECK(response->status == 200);
    CHECK(response->text() == "{}");
}

TEST_CASE("bounded HTTPS does not bypass certificate host validation") {
    TlsServer server{"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}"};
    const std::array endpoints{server.endpoint()};
    const auth::HttpsOptions options{.deadline = std::chrono::steady_clock::now() + 2s,
                                     .resolvedAddresses = endpoints,
                                     .trustPem = auth::test::kLocalhostCertPem};
    const auto response = auth::httpsGet(server.url("other.invalid"), 64, {}, options);
    REQUIRE_FALSE(response);
    CHECK(response.error().kind == auth::HttpsError::Kind::Tls);
}

TEST_CASE("bounded HTTPS times out a peer that never completes TLS") {
    TlsServer server{"", true};
    const std::array endpoints{server.endpoint()};
    const auth::HttpsOptions options{.deadline = std::chrono::steady_clock::now() + 100ms,
                                     .resolvedAddresses = endpoints,
                                     .trustPem = auth::test::kLocalhostCertPem};
    const auto response = auth::httpsGet(server.url(), 64, {}, options);
    REQUIRE_FALSE(response);
    CHECK(response.error().kind == auth::HttpsError::Kind::TimedOut);
}

TEST_CASE("canceling a TLS handshake wakes its readiness wait before the deadline") {
    TlsServer server{"", true};
    const std::array endpoints{server.endpoint()};
    std::stop_source stop;
    const auth::HttpsOptions options{.deadline = std::chrono::steady_clock::now() + 5s,
                                     .stop = stop.get_token(),
                                     .resolvedAddresses = endpoints,
                                     .trustPem = auth::test::kLocalhostCertPem};
    std::optional<std::expected<auth::HttpsResponse, auth::HttpsError>> result;
    std::binary_semaphore finished{0};
    std::jthread client([&] {
        result.emplace(auth::httpsGet(server.url(), 64, {}, options));
        finished.release();
    });
    REQUIRE(server.accepted());
    stop.request_stop();
    REQUIRE(finished.try_acquire_for(2s));
    client.join();
    REQUIRE(result);
    REQUIRE_FALSE(*result);
    CHECK(result->error().kind == auth::HttpsError::Kind::Canceled);
}

TEST_CASE("TLS responses cannot turn a body limit into successful truncation") {
    TlsServer server{"HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n" + std::string(100, 'x')};
    const std::array endpoints{server.endpoint()};
    const auth::HttpsOptions options{.deadline = std::chrono::steady_clock::now() + 2s,
                                     .resolvedAddresses = endpoints,
                                     .trustPem = auth::test::kLocalhostCertPem};
    CHECK_FALSE(auth::httpsGet(server.url(), 64, {}, options));
}

TEST_CASE("invalid URLs and canceled HTTPS calls are rejected before network activity") {
    for (const std::string_view url :
         {"http://localhost/", "https://localhost:bad/", "https://localhost:0/", "https://localhost:65536/",
          "https://user@localhost/", "https://localhost/\r\nInjected: header", "https://[::1]:bad/"}) {
        const auto result = auth::httpsGet(url, 64);
        REQUIRE_FALSE(result);
        CHECK(result.error().kind == auth::HttpsError::Kind::InvalidRequest);
    }
    std::stop_source stop;
    stop.request_stop();
    const auto canceled = auth::httpsGet("https://localhost/", 64, {}, {.stop = stop.get_token()});
    REQUIRE_FALSE(canceled);
    CHECK(canceled.error().kind == auth::HttpsError::Kind::Canceled);
    const auto expired =
        auth::httpsGet("https://localhost/", 64, {}, {.deadline = std::chrono::steady_clock::now() - 1ms});
    REQUIRE_FALSE(expired);
    CHECK(expired.error().kind == auth::HttpsError::Kind::TimedOut);
}

TEST_CASE("a bounded HTTPS request never falls back to synchronous DNS") {
    const auto result = auth::httpsGet("https://must-not-resolve.invalid/", 64, {},
                                       {.deadline = std::chrono::steady_clock::now() + 2s});
    REQUIRE_FALSE(result);
    CHECK(result.error().kind == auth::HttpsError::Kind::InvalidRequest);
}

TEST_CASE("TLS authentication does not accept a truncated successful profile body") {
    TlsServer server{"HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\n{}"};
    const std::array endpoints{server.endpoint()};
    const auth::HttpsOptions options{.deadline = std::chrono::steady_clock::now() + 2s,
                                     .resolvedAddresses = endpoints,
                                     .trustPem = auth::test::kLocalhostCertPem};
    CHECK_FALSE(auth::httpsGet(server.url(), 64, {}, options));
}

TEST_CASE("canceling HTTPS body reads retires a stalled authenticated connection") {
    TlsServer server{"HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\n{", false, true};
    const std::array endpoints{server.endpoint()};
    std::stop_source stop;
    const auth::HttpsOptions options{.deadline = std::chrono::steady_clock::now() + 5s,
                                     .stop = stop.get_token(),
                                     .resolvedAddresses = endpoints,
                                     .trustPem = auth::test::kLocalhostCertPem};
    std::optional<std::expected<auth::HttpsResponse, auth::HttpsError>> result;
    std::binary_semaphore finished{0};
    std::jthread client([&] {
        result.emplace(auth::httpsGet(server.url(), 64, {}, options));
        finished.release();
    });
    REQUIRE(server.responseSent());
    stop.request_stop();
    REQUIRE(finished.try_acquire_for(2s));
    client.join();
    REQUIRE(result);
    REQUIRE_FALSE(*result);
    CHECK(result->error().kind == auth::HttpsError::Kind::Canceled);
}

TEST_CASE("bounded HTTPS can use an IPv6 endpoint while verifying its DNS certificate name") {
    TlsServer server{"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}", false, false, true};
    const std::array endpoints{server.endpoint()};
    const auth::HttpsOptions options{.deadline = std::chrono::steady_clock::now() + 2s,
                                     .resolvedAddresses = endpoints,
                                     .trustPem = auth::test::kLocalhostCertPem};
    const auto result = auth::httpsGet(server.url(), 64, {}, options);
    REQUIRE(result);
    CHECK(result->text() == "{}");
}
