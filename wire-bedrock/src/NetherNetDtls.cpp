#include "NetherNetDtls.hpp"

#include <algorithm>
#include <array>
#include <basalt_mbedtls_threads.h>
#include <cctype>
#include <chrono>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <utility>

namespace basalt::wire::bedrock::nethernet {
namespace {

using Clock = std::chrono::steady_clock;

constexpr unsigned kMtu = 1400;

constexpr std::array<int, 4> kSuites = {MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
                                        MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
                                        MBEDTLS_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0};

std::string colonHex(std::span<const std::uint8_t> bytes) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string out;
    for (const std::uint8_t byte : bytes) {
        if (!out.empty()) {
            out += ':';
        }
        out += kDigits[byte >> 4U];
        out += kDigits[byte & 0x0fU];
    }
    return out;
}

std::string upper(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

std::string digestOf(const mbedtls_x509_crt* certificate) {
    std::array<std::uint8_t, 32> digest{};
    mbedtls_sha256(certificate->raw.p, certificate->raw.len, digest.data(), 0);
    return colonHex(digest);
}

} // namespace

struct DtlsCredentials::State {
    mbedtls_entropy_context entropy{};
    mbedtls_ctr_drbg_context random{};
    mbedtls_pk_context key{};
    mbedtls_x509_crt certificate{};
    mbedtls_ssl_config config{};
    std::string fingerprint;
    bool ready = false;

    State() {
        basalt_mbedtls_threading_init();
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&random);
        mbedtls_pk_init(&key);
        mbedtls_x509_crt_init(&certificate);
        mbedtls_ssl_config_init(&config);
    }

    ~State() {
        mbedtls_ssl_config_free(&config);
        mbedtls_x509_crt_free(&certificate);
        mbedtls_pk_free(&key);
        mbedtls_ctr_drbg_free(&random);
        mbedtls_entropy_free(&entropy);
    }

    State(const State&) = delete;
    State& operator=(const State&) = delete;
};

DtlsCredentials::DtlsCredentials() : state_(std::make_unique<State>()) {
    State& state = *state_;
    constexpr std::string_view kPersonalisation = "basalt-nethernet";
    if (mbedtls_ctr_drbg_seed(&state.random, mbedtls_entropy_func, &state.entropy,
                              reinterpret_cast<const unsigned char*>(kPersonalisation.data()),
                              kPersonalisation.size()) != 0 ||
        mbedtls_pk_setup(&state.key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0 ||
        mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(state.key), mbedtls_ctr_drbg_random,
                            &state.random) != 0) {
        return;
    }

    mbedtls_x509write_cert writer;
    mbedtls_x509write_crt_init(&writer);
    constexpr std::array<unsigned char, 8> kSerial = {0x42, 0x61, 0x73, 0x61, 0x6c, 0x74, 0x4e, 0x4e};
    mbedtls_x509write_crt_set_md_alg(&writer, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&writer, &state.key);
    mbedtls_x509write_crt_set_issuer_key(&writer, &state.key);
    std::array<unsigned char, 1024> der{};
    const bool built = mbedtls_x509write_crt_set_subject_name(&writer, "CN=Basalt NetherNet") == 0 &&
                       mbedtls_x509write_crt_set_issuer_name(&writer, "CN=Basalt NetherNet") == 0 &&
                       mbedtls_x509write_crt_set_serial_raw(&writer, const_cast<unsigned char*>(kSerial.data()),
                                                            kSerial.size()) == 0 &&
                       mbedtls_x509write_crt_set_validity(&writer, "20250101000000", "20450101000000") == 0;
    int written =
        built ? mbedtls_x509write_crt_der(&writer, der.data(), der.size(), mbedtls_ctr_drbg_random, &state.random) : -1;
    mbedtls_x509write_crt_free(&writer);
    if (written <= 0) {
        return;
    }
    if (mbedtls_x509_crt_parse_der(&state.certificate, der.data() + der.size() - static_cast<std::size_t>(written),
                                   static_cast<std::size_t>(written)) != 0) {
        return;
    }
    state.fingerprint = digestOf(&state.certificate);

    if (mbedtls_ssl_config_defaults(&state.config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        return;
    }
    mbedtls_ssl_conf_min_tls_version(&state.config, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&state.config, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_rng(&state.config, mbedtls_ctr_drbg_random, &state.random);
    mbedtls_ssl_conf_ciphersuites(&state.config, kSuites.data());
    mbedtls_ssl_conf_authmode(&state.config, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_handshake_timeout(&state.config, 1000, 8000);
    if (mbedtls_ssl_conf_own_cert(&state.config, &state.certificate, &state.key) != 0) {
        return;
    }
    state.ready = true;
}

DtlsCredentials::~DtlsCredentials() = default;

bool DtlsCredentials::ok() const noexcept { return state_->ready; }

const std::string& DtlsCredentials::fingerprint() const noexcept { return state_->fingerprint; }

struct DtlsSession::State {
    mbedtls_ssl_context ssl{};
    std::string expected;
    Sender sender;
    std::deque<std::vector<std::uint8_t>> inbox;
    Clock::time_point timerStart{};
    std::uint32_t intermediateMs = 0;
    std::uint32_t finalMs = 0;
    bool timerRunning = false;
    bool closed = false;
    bool fingerprintMatched = false;

    static int sendCallback(void* context, const unsigned char* data, std::size_t length) {
        auto& self = *static_cast<State*>(context);
        self.sender({data, length});
        return static_cast<int>(length);
    }

    static int receiveCallback(void* context, unsigned char* buffer, std::size_t capacity) {
        auto& self = *static_cast<State*>(context);
        if (self.inbox.empty()) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
        const std::vector<std::uint8_t>& front = self.inbox.front();
        const std::size_t count = std::min(capacity, front.size());
        std::copy_n(front.begin(), count, buffer);
        self.inbox.pop_front();
        return static_cast<int>(count);
    }

    static void setTimer(void* context, std::uint32_t intermediate, std::uint32_t final) {
        auto& self = *static_cast<State*>(context);
        self.intermediateMs = intermediate;
        self.finalMs = final;
        self.timerRunning = final != 0;
        self.timerStart = Clock::now();
    }

    static int getTimer(void* context) {
        const auto& self = *static_cast<State*>(context);
        if (!self.timerRunning) {
            return -1;
        }
        const auto elapsed = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - self.timerStart).count());
        if (elapsed >= self.finalMs) {
            return 2;
        }
        return elapsed >= self.intermediateMs ? 1 : 0;
    }

    static int verify(void* context, mbedtls_x509_crt* certificate, int depth, std::uint32_t* flags) {
        auto& self = *static_cast<State*>(context);
        if (depth != 0) {
            *flags = 0;
            return 0;
        }
        if (upper(self.expected) != digestOf(certificate)) {
            return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
        }
        *flags = 0;
        self.fingerprintMatched = true;
        return 0;
    }

    State() { mbedtls_ssl_init(&ssl); }

    ~State() { mbedtls_ssl_free(&ssl); }

    State(const State&) = delete;
    State& operator=(const State&) = delete;
};

DtlsSession::DtlsSession(DtlsCredentials& credentials, std::string expectedFingerprint, Sender sender)
    : state_(std::make_unique<State>()) {
    State& state = *state_;
    state.expected = std::move(expectedFingerprint);
    state.sender = std::move(sender);
    if (mbedtls_ssl_setup(&state.ssl, &credentials.state().config) != 0) {
        failed_ = true;
        return;
    }
    mbedtls_ssl_set_hostname(&state.ssl, nullptr);
    mbedtls_ssl_set_mtu(&state.ssl, kMtu);
    mbedtls_ssl_set_bio(&state.ssl, &state, State::sendCallback, State::receiveCallback, nullptr);
    mbedtls_ssl_set_timer_cb(&state.ssl, &state, State::setTimer, State::getTimer);
    mbedtls_ssl_set_verify(&state.ssl, State::verify, &state);
}

DtlsSession::~DtlsSession() = default;

void DtlsSession::feed(std::span<const std::uint8_t> datagram) {
    state_->inbox.emplace_back(datagram.begin(), datagram.end());
}

void DtlsSession::pump(std::vector<std::vector<std::uint8_t>>& received) {
    State& state = *state_;
    if (failed_ || state.closed) {
        return;
    }
    if (!established_) {
        const int code = mbedtls_ssl_handshake(&state.ssl);
        if (code == 0) {
            established_ = state.fingerprintMatched;
            failed_ = !state.fingerprintMatched;
        } else if (code != MBEDTLS_ERR_SSL_WANT_READ && code != MBEDTLS_ERR_SSL_WANT_WRITE &&
                   code != MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
            failed_ = true;
            return;
        }
        if (!established_) {
            return;
        }
    }

    std::array<std::uint8_t, 4096> buffer{};
    while (true) {
        const int count = mbedtls_ssl_read(&state.ssl, buffer.data(), buffer.size());
        if (count > 0) {
            received.emplace_back(buffer.begin(), buffer.begin() + count);
            continue;
        }
        if (count == MBEDTLS_ERR_SSL_WANT_READ || count == MBEDTLS_ERR_SSL_WANT_WRITE) {
            return;
        }
        failed_ = true;
        return;
    }
}

bool DtlsSession::send(std::span<const std::uint8_t> data) {
    if (!established_ || failed_ || state_->closed) {
        return false;
    }
    const int written = mbedtls_ssl_write(&state_->ssl, data.data(), data.size());
    return written == static_cast<int>(data.size());
}

void DtlsSession::close() {
    if (state_->closed) {
        return;
    }
    if (established_ && !failed_) {
        mbedtls_ssl_close_notify(&state_->ssl);
    }
    state_->closed = true;
}

} // namespace basalt::wire::bedrock::nethernet
