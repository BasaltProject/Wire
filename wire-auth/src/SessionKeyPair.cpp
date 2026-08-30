#include <basalt/wire/auth/SessionKeyPair.hpp>

#include <array>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha1.h>
#include <mutex>

namespace basalt::wire::auth {
namespace {

constexpr unsigned int kRsaKeyBits = 1024;
constexpr int kRsaExponent = 65537;
constexpr std::string_view kSeed = "basalt-session";

/// One seeded generator for the process. Seeding costs entropy and time, and every use here is a
/// few bytes at a time.
struct Rng {
    mbedtls_entropy_context entropy{};
    mbedtls_ctr_drbg_context drbg{};
    bool ready = false;

    Rng() {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        ready = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                                      reinterpret_cast<const unsigned char*>(kSeed.data()), kSeed.size()) == 0;
    }

    ~Rng() {
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
    }

    Rng(const Rng&) = delete;
    Rng& operator=(const Rng&) = delete;
};

Rng& rng() {
    static Rng instance;
    return instance;
}

std::mutex& rngMutex() {
    static std::mutex instance;
    return instance;
}

} // namespace

struct SessionKeyPair::State {
    mbedtls_pk_context key{};
    std::vector<std::uint8_t> publicKeyDer;

    State() { mbedtls_pk_init(&key); }

    ~State() { mbedtls_pk_free(&key); }

    State(const State&) = delete;
    State& operator=(const State&) = delete;
};

SessionKeyPair::SessionKeyPair() : state_(std::make_unique<State>()) {}

SessionKeyPair::SessionKeyPair(SessionKeyPair&&) noexcept = default;

SessionKeyPair& SessionKeyPair::operator=(SessionKeyPair&&) noexcept = default;

SessionKeyPair::~SessionKeyPair() = default;

std::optional<SessionKeyPair> SessionKeyPair::generate() {
    if (!rng().ready) {
        return std::nullopt;
    }

    SessionKeyPair pair;
    const std::scoped_lock lock{rngMutex()};

    if (mbedtls_pk_setup(&pair.state_->key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)) != 0) {
        return std::nullopt;
    }
    if (mbedtls_rsa_gen_key(mbedtls_pk_rsa(pair.state_->key), mbedtls_ctr_drbg_random, &rng().drbg, kRsaKeyBits,
                            kRsaExponent) != 0) {
        return std::nullopt;
    }

    // mbedtls_pk_write_pubkey_der fills the buffer from the back and returns the length written.
    std::array<unsigned char, 512> scratch{};
    const int written = mbedtls_pk_write_pubkey_der(&pair.state_->key, scratch.data(), scratch.size());
    if (written < 0) {
        return std::nullopt;
    }
    const auto length = static_cast<std::size_t>(written);
    pair.state_->publicKeyDer.assign(scratch.end() - static_cast<std::ptrdiff_t>(length), scratch.end());
    return pair;
}

std::span<const std::uint8_t> SessionKeyPair::publicKeyDer() const noexcept { return state_->publicKeyDer; }

std::optional<std::vector<std::uint8_t>> SessionKeyPair::decrypt(std::span<const std::uint8_t> ciphertext) const {
    std::array<unsigned char, 512> plain{};
    std::size_t length = 0;

    const std::scoped_lock lock{rngMutex()};
    const int result = mbedtls_pk_decrypt(&state_->key, ciphertext.data(), ciphertext.size(), plain.data(), &length,
                                          plain.size(), mbedtls_ctr_drbg_random, &rng().drbg);
    if (result != 0) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>{plain.begin(), plain.begin() + static_cast<std::ptrdiff_t>(length)};
}

bool randomBytes(std::span<std::uint8_t> into) {
    if (!rng().ready) {
        return false;
    }
    const std::scoped_lock lock{rngMutex()};
    return mbedtls_ctr_drbg_random(&rng().drbg, into.data(), into.size()) == 0;
}

std::string formatSessionHash(std::span<const std::uint8_t> digest) {
    if (digest.empty()) {
        return "0";
    }

    const bool negative = (digest[0] & 0x80) != 0;
    std::vector<std::uint8_t> magnitude{digest.begin(), digest.end()};
    if (negative) {
        // Two's-complement negation, so the value prints as `-<magnitude>` the way BigInteger does.
        int carry = 1;
        for (std::size_t i = magnitude.size(); i-- > 0;) {
            const int sum = (magnitude[i] ^ 0xFF) + carry;
            magnitude[i] = static_cast<std::uint8_t>(sum);
            carry = sum >> 8;
        }
    }

    static constexpr char kDigits[] = "0123456789abcdef";
    std::string hex;
    bool leading = true;
    for (const std::uint8_t byte : magnitude) {
        if (leading && byte == 0) {
            continue;
        }
        if (leading && (byte >> 4) == 0) {
            hex += kDigits[byte & 0xF];
        } else {
            hex += kDigits[byte >> 4];
            hex += kDigits[byte & 0xF];
        }
        leading = false;
    }
    if (hex.empty()) {
        return "0";
    }
    return negative ? "-" + hex : hex;
}

std::string sessionHash(std::string_view serverId, std::span<const std::uint8_t> secretKey,
                        std::span<const std::uint8_t> publicKeyDer) {
    mbedtls_sha1_context context{};
    mbedtls_sha1_init(&context);
    mbedtls_sha1_starts(&context);

    // ISO-8859-1, which for the empty server id modern versions send is the same as nothing at all,
    // but the encoding is part of the parity contract.
    for (const char c : serverId) {
        const auto byte = static_cast<unsigned char>(c);
        mbedtls_sha1_update(&context, &byte, 1);
    }
    mbedtls_sha1_update(&context, secretKey.data(), secretKey.size());
    mbedtls_sha1_update(&context, publicKeyDer.data(), publicKeyDer.size());

    std::array<unsigned char, 20> digest{};
    mbedtls_sha1_finish(&context, digest.data());
    mbedtls_sha1_free(&context);

    return formatSessionHash(std::span<const std::uint8_t>{digest});
}

} // namespace basalt::wire::auth
