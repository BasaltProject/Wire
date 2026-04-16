#include "HttpsResponseDecoder.hpp"

#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/io/EventLoop.hpp>
#include <basalt/wire/io/TcpSocket.hpp>

#include <basalt_mbedtls_threads.h>

extern "C" {
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
}

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <limits>

namespace basalt::wire::auth {
namespace {

/// Where each platform keeps the certificates it trusts. The first one that reads is used.
///
/// A bundle rather than a directory: mbedTLS parses a concatenated PEM file in one call, and every
/// system that ships a directory also ships the bundle beside it.
constexpr std::array<const char*, 6> kTrustStores{
    "/etc/ssl/cert.pem",                      // macOS, and OpenSSL's own default
    "/etc/ssl/certs/ca-certificates.crt",     // Debian, Ubuntu, Alpine
    "/etc/pki/tls/certs/ca-bundle.crt",       // Fedora, RHEL
    "/etc/ssl/ca-bundle.pem",                 // openSUSE
    "/etc/pki/tls/cacert.pem",                // older RHEL
    "/usr/local/share/certs/ca-root-nss.crt", // FreeBSD
};

struct Url {
    std::string host;
    std::string port;
    std::string path;
    std::string authority;
};

/// Splits `https://host[:port]/path`. Nothing for anything else, `http://` included: this fetches
/// jars and profiles, and neither is worth doing in the clear.
[[nodiscard]] std::optional<Url> parse(std::string_view url) {
    constexpr std::string_view kScheme = "https://";
    if (!url.starts_with(kScheme) || std::ranges::any_of(url, [](unsigned char c) { return c <= 32 || c == 127; })) {
        return std::nullopt;
    }
    url.remove_prefix(kScheme.size());
    url = url.substr(0, url.find('#'));
    const auto boundary = url.find_first_of("/?");
    const auto authority = url.substr(0, boundary);
    if (authority.empty() || authority.find('@') != std::string_view::npos) {
        return std::nullopt;
    }
    Url parsed;
    parsed.authority = authority;
    parsed.path = boundary == std::string_view::npos ? "/" : std::string{url.substr(boundary)};
    if (parsed.path.front() == '?') {
        parsed.path.insert(parsed.path.begin(), '/');
    }
    parsed.port = "443";
    if (authority.front() == '[') {
        const auto bracket = authority.find(']');
        if (bracket == std::string_view::npos || bracket <= 1) {
            return std::nullopt;
        }
        parsed.host = authority.substr(1, bracket - 1);
        if (parsed.host.find(':') == std::string::npos) {
            return std::nullopt;
        }
        if (bracket + 1 < authority.size()) {
            if (authority[bracket + 1] != ':') {
                return std::nullopt;
            }
            parsed.port = authority.substr(bracket + 2);
        }
    } else {
        const auto colon = authority.find(':');
        parsed.host = authority.substr(0, colon);
        if (colon != std::string_view::npos) {
            parsed.port = authority.substr(colon + 1);
        }
    }
    int port = 0;
    const auto number = std::from_chars(parsed.port.data(), parsed.port.data() + parsed.port.size(), port);
    if (parsed.host.empty() || number.ec != std::errc{} || number.ptr != parsed.port.data() + parsed.port.size() ||
        port < 1 || port > 65535) {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] std::string describe(int code) {
    std::array<char, 128> written{};
    mbedtls_strerror(code, written.data(), written.size());
    return std::string{written.data()};
}

/// Everything one request needs, freed in reverse whatever happens.
class Session {
public:
    explicit Session(const HttpsOptions& options) : options_(options) {
        basalt_mbedtls_threading_init();
        mbedtls_ssl_init(&ssl_);
        mbedtls_ssl_config_init(&config_);
        mbedtls_x509_crt_init(&trusted_);
        mbedtls_ctr_drbg_init(&random_);
        mbedtls_entropy_init(&entropy_);
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    ~Session() {
        mbedtls_ssl_close_notify(&ssl_);
        mbedtls_entropy_free(&entropy_);
        mbedtls_ctr_drbg_free(&random_);
        mbedtls_x509_crt_free(&trusted_);
        mbedtls_ssl_config_free(&config_);
        mbedtls_ssl_free(&ssl_);
    }

    void wake() { static_cast<void>(loop_.wake()); }

    std::optional<HttpsError> controlError() const { return check(); }

    [[nodiscard]] std::optional<HttpsError> connect(const Url& url) {
        if (const auto failed = check()) {
            return failed;
        }
        static constexpr std::string_view kSeed = "basalt-https";
        int code = mbedtls_ctr_drbg_seed(&random_, mbedtls_entropy_func, &entropy_,
                                         reinterpret_cast<const unsigned char*>(kSeed.data()), kSeed.size());
        if (code != 0) {
            return HttpsError{"could not seed the random source: " + describe(code)};
        }
        bool anyTrusted = false;
        if (!options_.trustPem.empty()) {
            const std::string pem{options_.trustPem};
            anyTrusted = mbedtls_x509_crt_parse(&trusted_, reinterpret_cast<const unsigned char*>(pem.c_str()),
                                                pem.size() + 1) == 0;
        }
        for (const char* store : kTrustStores) {
            if (!options_.trustPem.empty()) {
                break;
            }
            if (mbedtls_x509_crt_parse_file(&trusted_, store) == 0) {
                anyTrusted = true;
                break;
            }
        }
        if (!anyTrusted) {
            // Refused rather than proceeding unverified: an unauthenticated fetch of a jar is how a
            // supply chain gets compromised.
            return HttpsError{"no certificate store was found, so the server could not be verified"};
        }
        if (const auto failed = connectSocket(url)) {
            return failed;
        }
        code = mbedtls_ssl_config_defaults(&config_, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT);
        if (code != 0) {
            return HttpsError{"could not configure TLS: " + describe(code)};
        }
        mbedtls_ssl_conf_authmode(&config_, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&config_, &trusted_, nullptr);
        mbedtls_ssl_conf_rng(&config_, mbedtls_ctr_drbg_random, &random_);
        code = mbedtls_ssl_setup(&ssl_, &config_);
        if (code != 0) {
            return HttpsError{"could not set up TLS: " + describe(code)};
        }
        // The name goes out in SNI and is what the certificate is checked against.
        code = mbedtls_ssl_set_hostname(&ssl_, url.host.c_str());
        if (code != 0) {
            return HttpsError{"could not set the TLS host name: " + describe(code)};
        }
        mbedtls_ssl_set_bio(&ssl_, this, &Session::send, &Session::receive, nullptr);
        while ((code = mbedtls_ssl_handshake(&ssl_)) != 0) {
            if (code == MBEDTLS_ERR_SSL_WANT_READ || code == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (const auto failed = wait(code)) {
                    return failed;
                }
                continue;
            }
            if (code != MBEDTLS_ERR_SSL_WANT_READ && code != MBEDTLS_ERR_SSL_WANT_WRITE) {
                return HttpsError{"the TLS handshake with " + url.host + " failed: " + describe(code),
                                  HttpsError::Kind::Tls};
            }
        }
        if (mbedtls_ssl_get_verify_result(&ssl_) != 0) {
            return HttpsError{url.host + " presented a certificate that could not be verified", HttpsError::Kind::Tls};
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<HttpsError> write(std::string_view request) {
        std::size_t sent = 0;
        while (sent < request.size()) {
            if (const auto failed = check()) {
                return failed;
            }
            const int written = mbedtls_ssl_write(&ssl_, reinterpret_cast<const unsigned char*>(request.data() + sent),
                                                  request.size() - sent);
            if (written == MBEDTLS_ERR_SSL_WANT_READ || written == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (const auto failed = wait(written)) {
                    return failed;
                }
                continue;
            }
            if (written <= 0) {
                return HttpsError{"the request could not be sent: " + describe(written)};
            }
            sent += static_cast<std::size_t>(written);
        }
        return std::nullopt;
    }

    /// Reads until the peer closes or the ceiling is reached. Answers what came back.
    [[nodiscard]] std::expected<std::vector<std::uint8_t>, HttpsError> readAll(std::size_t maxBytes,
                                                                               const HttpsProgress& onProgress) {
        std::vector<std::uint8_t> answered;
        std::array<unsigned char, 16384> chunk{};
        while (true) {
            if (const auto failed = check()) {
                return std::unexpected{*failed};
            }
            // Probe one byte at the ceiling to distinguish EOF from a silently truncated response.
            const std::size_t room = maxBytes - answered.size();
            const std::size_t requested = room == 0 ? 1 : std::min(chunk.size(), room);
            const int read = mbedtls_ssl_read(&ssl_, chunk.data(), requested);
            if (read == MBEDTLS_ERR_SSL_WANT_READ || read == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (const auto failed = wait(read)) {
                    return std::unexpected{*failed};
                }
                continue;
            }
            if (read == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || read == 0) {
                return answered;
            }
            if (read < 0) {
                return std::unexpected{HttpsError{"the response could not be read: " + describe(read)}};
            }
            if (room == 0) {
                return std::unexpected{HttpsError{"the response exceeded its encoded byte limit"}};
            }
            answered.insert(answered.end(), chunk.begin(), chunk.begin() + read);
            if (onProgress) {
                onProgress(answered.size());
            }
        }
    }

private:
    [[nodiscard]] std::optional<HttpsError> check() const {
        if (options_.stop.stop_requested()) {
            return HttpsError{"the request was canceled", HttpsError::Kind::Canceled};
        }
        if (options_.deadline && std::chrono::steady_clock::now() >= *options_.deadline) {
            return HttpsError{"the request deadline expired", HttpsError::Kind::TimedOut};
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<HttpsError> wait(int wanted) {
        if (const auto failed = check()) {
            return failed;
        }
        const auto watched = loop_.watch(
            socket_.nativeHandle(), {wanted == MBEDTLS_ERR_SSL_WANT_READ, wanted == MBEDTLS_ERR_SSL_WANT_WRITE}, 1);
        if (!watched) {
            return HttpsError{"could not watch the TLS socket"};
        }
        std::optional<int> timeout;
        if (options_.deadline) {
            const auto remaining =
                std::chrono::ceil<std::chrono::milliseconds>(*options_.deadline - std::chrono::steady_clock::now());
            timeout = static_cast<int>(std::clamp<std::int64_t>(remaining.count(), 0, std::numeric_limits<int>::max()));
        }
        const auto events = loop_.wait(timeout);
        if (!events) {
            return HttpsError{"could not wait for the TLS socket"};
        }
        return check();
    }

    [[nodiscard]] std::optional<HttpsError> connectSocket(const Url& url) {
        if (const auto failed = check()) {
            return failed;
        }
        int port = 0;
        const auto number = std::from_chars(url.port.data(), url.port.data() + url.port.size(), port);
        if (number.ec != std::errc{} || number.ptr != url.port.data() + url.port.size() || port < 1 || port > 65535) {
            return HttpsError{"invalid HTTPS port", HttpsError::Kind::InvalidRequest};
        }
        std::vector<io::TcpAddress> systemAddresses;
        auto addresses = options_.resolvedAddresses;
        if (addresses.empty()) {
            if (options_.deadline || options_.stop.stop_possible()) {
                return HttpsError{"bounded HTTPS requires pre-resolved endpoints", HttpsError::Kind::InvalidRequest};
            }
            const auto resolved = io::TcpAddress::resolve(url.host, static_cast<std::uint16_t>(port));
            if (!resolved) {
                return HttpsError{"could not resolve the HTTPS host"};
            }
            systemAddresses = *resolved;
            addresses = systemAddresses;
        }
        for (const auto& address : addresses) {
            if (const auto failed = check()) {
                return failed;
            }
            if (address.port() != port) {
                return HttpsError{"resolved endpoint has the wrong port", HttpsError::Kind::InvalidRequest};
            }
            auto connection = io::TcpConnection::connectNonBlocking(address);
            if (!connection) {
                continue;
            }
            socket_ = std::move(*connection);
            while (true) {
                const auto connected = socket_.finishConnect();
                if (!connected) {
                    break;
                }
                if (*connected) {
                    return std::nullopt;
                }
                if (const auto failed = wait(MBEDTLS_ERR_SSL_WANT_WRITE)) {
                    return failed;
                }
            }
            loop_.unwatch(socket_.nativeHandle());
            socket_.close();
        }
        return HttpsError{"could not connect to the HTTPS host"};
    }

    static int send(void* context, const unsigned char* bytes, std::size_t length) {
        auto& session = *static_cast<Session*>(context);
        const auto sent = session.socket_.writeSome({bytes, length});
        if (!sent) {
            return sent.error().kind == io::SocketError::Kind::WouldBlock ? MBEDTLS_ERR_SSL_WANT_WRITE
                                                                          : MBEDTLS_ERR_NET_SEND_FAILED;
        }
        return static_cast<int>(*sent);
    }

    static int receive(void* context, unsigned char* bytes, std::size_t length) {
        auto& session = *static_cast<Session*>(context);
        const auto read = session.socket_.read({bytes, length});
        if (!read) {
            return read.error().kind == io::SocketError::Kind::WouldBlock ? MBEDTLS_ERR_SSL_WANT_READ
                                                                          : MBEDTLS_ERR_NET_RECV_FAILED;
        }
        return static_cast<int>(*read);
    }

    const HttpsOptions& options_;
    io::TcpConnection socket_;
    io::EventLoop loop_;
    mbedtls_ssl_context ssl_{};
    mbedtls_ssl_config config_{};
    mbedtls_x509_crt trusted_{};
    mbedtls_ctr_drbg_context random_{};
    mbedtls_entropy_context entropy_{};
};

} // namespace

std::expected<HttpsResponse, HttpsError> detail::decodeHttpsResponse(std::span<const std::uint8_t> raw,
                                                                     std::size_t maxBodyBytes) {
    constexpr std::size_t kEnvelopeBudget = 64 * 1024;
    const auto error = [](const char* message) -> std::expected<HttpsResponse, HttpsError> {
        return std::unexpected{HttpsError{message}};
    };
    if (maxBodyBytes > std::numeric_limits<std::size_t>::max() - kEnvelopeBudget ||
        raw.size() > maxBodyBytes + kEnvelopeBudget) {
        return error("the response exceeded its encoded byte limit");
    }
    if (raw.empty()) {
        return error("the answer had no complete header");
    }
    const std::string_view whole{reinterpret_cast<const char*>(raw.data()), raw.size()};
    const std::size_t split = whole.find("\r\n\r\n");
    if (split == std::string_view::npos || split > kEnvelopeBudget) {
        return error("the answer had no bounded complete header");
    }
    const auto trim = [](std::string_view value) {
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.remove_prefix(1);
        }
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
            value.remove_suffix(1);
        }
        return value;
    };
    const auto equal = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i) {
            const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; };
            if (lower(a[i]) != lower(b[i])) {
                return false;
            }
        }
        return true;
    };
    std::string_view header = whole.substr(0, split);
    const auto firstLineEnd = header.find("\r\n");
    const auto first = header.substr(0, firstLineEnd);
    if ((!first.starts_with("HTTP/1.1 ") && !first.starts_with("HTTP/1.0 ")) || first.size() < 12 ||
        (first.size() > 12 && first[12] != ' ')) {
        return error("the answer had an invalid HTTP status line");
    }
    HttpsResponse response;
    const auto status = first.substr(9, 3);
    const auto parsedStatus = std::from_chars(status.data(), status.data() + status.size(), response.status);
    if (parsedStatus.ec != std::errc{} || parsedStatus.ptr != status.data() + status.size() || response.status < 100 ||
        response.status > 599) {
        return error("the answer had an invalid HTTP status");
    }
    header = firstLineEnd == std::string_view::npos ? std::string_view{} : header.substr(firstLineEnd + 2);
    std::optional<std::size_t> length;
    bool chunked = false;
    while (!header.empty()) {
        const auto end = header.find("\r\n");
        const auto line = header.substr(0, end);
        const auto colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            return error("the answer had an invalid header field");
        }
        const auto name = line.substr(0, colon);
        const auto value = trim(line.substr(colon + 1));
        const auto validName = [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                   std::string_view{"!#$%&'*+-.^_`|~"}.find(static_cast<char>(c)) != std::string_view::npos;
        };
        if (!std::ranges::all_of(name, validName) ||
            std::ranges::any_of(value, [](unsigned char c) { return (c < 32 && c != '\t') || c == 127; })) {
            return error("the answer had an invalid header field");
        }
        if (equal(name, "Content-Length")) {
            if (length || value.empty()) {
                return error("the answer had an ambiguous content length");
            }
            std::size_t size = 0;
            const auto number = std::from_chars(value.data(), value.data() + value.size(), size);
            if (number.ec != std::errc{} || number.ptr != value.data() + value.size()) {
                return error("the content length is invalid");
            }
            if (size > maxBodyBytes) {
                return error("the response exceeded its body byte limit");
            }
            length = size;
        } else if (equal(name, "Transfer-Encoding")) {
            if (chunked || !equal(value, "chunked")) {
                return error("the transfer encoding is unsupported or ambiguous");
            }
            chunked = true;
        }
        header = end == std::string_view::npos ? std::string_view{} : header.substr(end + 2);
    }
    std::string_view body = whole.substr(split + 4);
    if (chunked) {
        if (length) {
            return error("the answer mixed transfer encoding and content length");
        }
        while (true) {
            const auto end = body.find("\r\n");
            if (end == std::string_view::npos) {
                return error("the answer ended before its final chunk");
            }
            const auto line = body.substr(0, end);
            const auto digits = line.substr(0, line.find(';'));
            std::size_t size = 0;
            const auto number = std::from_chars(digits.data(), digits.data() + digits.size(), size, 16);
            if (digits.empty() || number.ec != std::errc{} || number.ptr != digits.data() + digits.size()) {
                return error("the chunk size is invalid");
            }
            body.remove_prefix(end + 2);
            if (size == 0) {
                if (body == "\r\n") {
                    return response;
                }
                const auto trailerEnd = body.find("\r\n\r\n");
                if (trailerEnd == std::string_view::npos || trailerEnd + 4 != body.size()) {
                    return error("the answer ended inside its trailers or carried extra data");
                }
                auto trailers = body.substr(0, trailerEnd);
                while (!trailers.empty()) {
                    const auto next = trailers.find("\r\n");
                    const auto trailer = trailers.substr(0, next);
                    const auto colon = trailer.find(':');
                    if (colon == std::string_view::npos || colon == 0 ||
                        equal(trailer.substr(0, colon), "Content-Length") ||
                        equal(trailer.substr(0, colon), "Transfer-Encoding")) {
                        return error("the trailers had an invalid field");
                    }
                    trailers = next == std::string_view::npos ? std::string_view{} : trailers.substr(next + 2);
                }
                return response;
            }
            if (size > maxBodyBytes - response.body.size()) {
                return error("the response exceeded its body byte limit");
            }
            if (body.size() < size || body.size() - size < 2 || body.substr(size, 2) != "\r\n") {
                return error("the answer ended inside a chunk or lacked its delimiter");
            }
            response.body.insert(response.body.end(), body.begin(), body.begin() + static_cast<std::ptrdiff_t>(size));
            body.remove_prefix(size + 2);
        }
    }
    if (length && *length != body.size()) {
        return error("the body did not match its content length");
    }
    if (body.size() > maxBodyBytes) {
        return error("the response exceeded its body byte limit");
    }
    response.body.assign(body.begin(), body.end());
    return response;
}

std::expected<HttpsResponse, HttpsError> httpsGet(std::string_view url, std::size_t maxBytes,
                                                  const HttpsProgress& onProgress, const HttpsOptions& options) {
    const std::optional<Url> target = parse(url);
    if (!target.has_value()) {
        return std::unexpected(HttpsError{"invalid HTTPS address", HttpsError::Kind::InvalidRequest});
    }
    constexpr std::size_t kEnvelopeBudget = 64 * 1024;
    if (maxBytes > std::numeric_limits<std::size_t>::max() - kEnvelopeBudget) {
        return std::unexpected{HttpsError{"the response byte limit is invalid"}};
    }
    if (options.stop.stop_requested()) {
        return std::unexpected{HttpsError{"the request was canceled", HttpsError::Kind::Canceled}};
    }
    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
        return std::unexpected{HttpsError{"the request deadline expired", HttpsError::Kind::TimedOut}};
    }
    if ((options.deadline || options.stop.stop_possible()) && options.resolvedAddresses.empty()) {
        return std::unexpected{
            HttpsError{"bounded HTTPS requires pre-resolved endpoints", HttpsError::Kind::InvalidRequest}};
    }
    Session session{options};
    std::stop_callback onCancel{options.stop, [&session] { session.wake(); }};
    if (const std::optional<HttpsError> failed = session.connect(*target); failed.has_value()) {
        return std::unexpected(*failed);
    }
    // `Connection: close` so the body ends at the socket, which is what makes reading it this simple.
    const std::string request = "GET " + target->path + " HTTP/1.1\r\nHost: " + target->authority +
                                "\r\nUser-Agent: Basalt\r\nAccept: */*\r\nConnection: close\r\n\r\n";
    if (const std::optional<HttpsError> failed = session.write(request); failed.has_value()) {
        return std::unexpected(*failed);
    }
    const auto raw = session.readAll(maxBytes + kEnvelopeBudget, onProgress);
    if (!raw) {
        return std::unexpected{raw.error()};
    }
    if (const auto failed = session.controlError()) {
        return std::unexpected{*failed};
    }
    auto response = detail::decodeHttpsResponse(*raw, maxBytes);
    if (const auto failed = session.controlError()) {
        return std::unexpected{*failed};
    }
    if (!response) {
        return response;
    }
    if (response->body.size() > maxBytes) {
        return std::unexpected{HttpsError{"the response exceeded its body byte limit"}};
    }
    return response;
}

} // namespace basalt::wire::auth
