#include <basalt/wire/auth/HttpsClient.hpp>

extern "C" {
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
}

#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>

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
};

/// Splits `https://host[:port]/path`. Nothing for anything else, `http://` included: this fetches
/// jars and profiles, and neither is worth doing in the clear.
[[nodiscard]] std::optional<Url> parse(std::string_view url) {
    constexpr std::string_view kScheme = "https://";
    if (!url.starts_with(kScheme)) {
        return std::nullopt;
    }
    url.remove_prefix(kScheme.size());
    const std::size_t slash = url.find('/');
    const std::string_view authority = slash == std::string_view::npos ? url : url.substr(0, slash);
    Url parsed;
    parsed.path = slash == std::string_view::npos ? "/" : std::string{url.substr(slash)};
    const std::size_t colon = authority.find(':');
    if (colon == std::string_view::npos) {
        parsed.host = authority;
        parsed.port = "443";
    } else {
        parsed.host = authority.substr(0, colon);
        parsed.port = authority.substr(colon + 1);
    }
    return parsed.host.empty() ? std::nullopt : std::optional{parsed};
}

[[nodiscard]] std::string describe(int code) {
    std::array<char, 128> written{};
    mbedtls_strerror(code, written.data(), written.size());
    return std::string{written.data()};
}

/// Everything one request needs, freed in reverse whatever happens.
class Session {
public:
    Session() {
        mbedtls_net_init(&socket_);
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
        mbedtls_net_free(&socket_);
    }

    [[nodiscard]] std::optional<HttpsError> connect(const Url& url) {
        static constexpr std::string_view kSeed = "basalt-https";
        int code = mbedtls_ctr_drbg_seed(&random_, mbedtls_entropy_func, &entropy_,
                                         reinterpret_cast<const unsigned char*>(kSeed.data()), kSeed.size());
        if (code != 0) {
            return HttpsError{"could not seed the random source: " + describe(code)};
        }
        bool anyTrusted = false;
        for (const char* store : kTrustStores) {
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
        code = mbedtls_net_connect(&socket_, url.host.c_str(), url.port.c_str(), MBEDTLS_NET_PROTO_TCP);
        if (code != 0) {
            return HttpsError{"could not reach " + url.host + ": " + describe(code)};
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
        mbedtls_ssl_set_bio(&ssl_, &socket_, mbedtls_net_send, mbedtls_net_recv, nullptr);
        while ((code = mbedtls_ssl_handshake(&ssl_)) != 0) {
            if (code != MBEDTLS_ERR_SSL_WANT_READ && code != MBEDTLS_ERR_SSL_WANT_WRITE) {
                return HttpsError{"the TLS handshake with " + url.host + " failed: " + describe(code)};
            }
        }
        if (mbedtls_ssl_get_verify_result(&ssl_) != 0) {
            return HttpsError{url.host + " presented a certificate that could not be verified"};
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<HttpsError> write(std::string_view request) {
        std::size_t sent = 0;
        while (sent < request.size()) {
            const int written = mbedtls_ssl_write(&ssl_, reinterpret_cast<const unsigned char*>(request.data() + sent),
                                                  request.size() - sent);
            if (written == MBEDTLS_ERR_SSL_WANT_READ || written == MBEDTLS_ERR_SSL_WANT_WRITE) {
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
    [[nodiscard]] std::vector<std::uint8_t> readAll(std::size_t maxBytes, const HttpsProgress& onProgress) {
        std::vector<std::uint8_t> answered;
        std::array<unsigned char, 16384> chunk{};
        while (answered.size() < maxBytes) {
            const int read = mbedtls_ssl_read(&ssl_, chunk.data(), chunk.size());
            if (read == MBEDTLS_ERR_SSL_WANT_READ || read == MBEDTLS_ERR_SSL_WANT_WRITE) {
                continue;
            }
            // A peer that closes cleanly and one that closes the socket both end the body.
            if (read == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || read <= 0) {
                break;
            }
            answered.insert(answered.end(), chunk.begin(), chunk.begin() + read);
            if (onProgress) {
                onProgress(answered.size());
            }
        }
        return answered;
    }

private:
    mbedtls_net_context socket_{};
    mbedtls_ssl_context ssl_{};
    mbedtls_ssl_config config_{};
    mbedtls_x509_crt trusted_{};
    mbedtls_ctr_drbg_context random_{};
    mbedtls_entropy_context entropy_{};
};

/// Splits the head from the body and reads the status line. Chunked bodies are decoded here too,
/// since asking the server not to chunk is not something HTTP/1.1 lets a client do.
[[nodiscard]] std::expected<HttpsResponse, HttpsError> decode(const std::vector<std::uint8_t>& raw) {
    const std::string_view whole{reinterpret_cast<const char*>(raw.data()), raw.size()};
    const std::size_t split = whole.find("\r\n\r\n");
    if (split == std::string_view::npos) {
        return std::unexpected(HttpsError{"the answer had no complete header"});
    }
    const std::string_view head = whole.substr(0, split);
    std::string_view body = whole.substr(split + 4);

    const std::size_t firstSpace = head.find(' ');
    if (firstSpace == std::string_view::npos) {
        return std::unexpected(HttpsError{"the answer had no status line"});
    }
    HttpsResponse response;
    const std::string_view status = head.substr(firstSpace + 1, 3);
    if (std::from_chars(status.data(), status.data() + status.size(), response.status).ec != std::errc{}) {
        return std::unexpected(HttpsError{"the answer had no status code"});
    }

    // A chunked body is a run of `<size in hex>\r\n<that many bytes>\r\n`, ending at a zero size.
    std::string lowered{head};
    for (char& letter : lowered) {
        letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    }
    if (lowered.find("transfer-encoding: chunked") != std::string::npos) {
        std::vector<std::uint8_t> joined;
        while (true) {
            const std::size_t lineEnd = body.find("\r\n");
            if (lineEnd == std::string_view::npos) {
                break;
            }
            std::size_t size = 0;
            const std::string_view sizeText = body.substr(0, lineEnd);
            if (std::from_chars(sizeText.data(), sizeText.data() + sizeText.size(), size, 16).ec != std::errc{}) {
                return std::unexpected(HttpsError{"a chunk of the answer had no size"});
            }
            if (size == 0) {
                break;
            }
            body.remove_prefix(lineEnd + 2);
            if (body.size() < size) {
                return std::unexpected(HttpsError{"the answer ended inside a chunk"});
            }
            joined.insert(joined.end(), body.begin(), body.begin() + static_cast<std::ptrdiff_t>(size));
            body.remove_prefix(size);
            if (body.starts_with("\r\n")) {
                body.remove_prefix(2);
            }
        }
        response.body = std::move(joined);
        return response;
    }
    response.body.assign(body.begin(), body.end());
    return response;
}

} // namespace

std::expected<HttpsResponse, HttpsError> httpsGet(std::string_view url, std::size_t maxBytes,
                                                  const HttpsProgress& onProgress) {
    const std::optional<Url> target = parse(url);
    if (!target.has_value()) {
        return std::unexpected(HttpsError{"'" + std::string{url} + "' is not an https:// address"});
    }
    Session session;
    if (const std::optional<HttpsError> failed = session.connect(*target); failed.has_value()) {
        return std::unexpected(*failed);
    }
    // `Connection: close` so the body ends at the socket, which is what makes reading it this simple.
    const std::string request = "GET " + target->path + " HTTP/1.1\r\nHost: " + target->host +
                                "\r\nUser-Agent: Basalt\r\nAccept: */*\r\nConnection: close\r\n\r\n";
    if (const std::optional<HttpsError> failed = session.write(request); failed.has_value()) {
        return std::unexpected(*failed);
    }
    return decode(session.readAll(maxBytes, onProgress));
}

} // namespace basalt::wire::auth
