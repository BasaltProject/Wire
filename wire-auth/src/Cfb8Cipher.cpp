#include <basalt/wire/auth/Cfb8Cipher.hpp>

#include <basalt_mbedtls_threads.h>
#include <cstdint>
#include <cstring>
#include <mbedtls/aes.h>

namespace basalt::wire::auth {
namespace {

constexpr std::size_t kBlockSize = 16;

} // namespace

struct Cfb8Cipher::KeySchedule {
    mbedtls_aes_context context{};

    KeySchedule() {
        basalt_mbedtls_threading_init();
        mbedtls_aes_init(&context);
    }

    ~KeySchedule() { mbedtls_aes_free(&context); }

    KeySchedule(const KeySchedule&) = delete;
    KeySchedule& operator=(const KeySchedule&) = delete;
};

Cfb8Cipher::Cfb8Cipher() : schedule_(std::make_unique<KeySchedule>()) {}

Cfb8Cipher::Cfb8Cipher(Cfb8Cipher&&) noexcept = default;

Cfb8Cipher& Cfb8Cipher::operator=(Cfb8Cipher&&) noexcept = default;

Cfb8Cipher::~Cfb8Cipher() = default;

std::optional<Cfb8Cipher> Cfb8Cipher::create(std::span<const std::uint8_t> key) {
    if (key.size() != kBlockSize) {
        return std::nullopt;
    }

    Cfb8Cipher cipher;
    if (mbedtls_aes_setkey_enc(&cipher.schedule_->context, key.data(), 128) != 0) {
        return std::nullopt;
    }
    std::memcpy(cipher.register_.data(), key.data(), kBlockSize);
    return cipher;
}

void Cfb8Cipher::update(std::span<std::uint8_t> data, bool encrypting) noexcept {
    std::array<std::uint8_t, kBlockSize> keystream{};
    for (std::uint8_t& byte : data) {
        mbedtls_aes_crypt_ecb(&schedule_->context, MBEDTLS_AES_ENCRYPT, register_.data(), keystream.data());
        const std::uint8_t input = byte;
        const auto output = static_cast<std::uint8_t>(input ^ keystream[0]);
        byte = output;

        std::memmove(register_.data(), register_.data() + 1, kBlockSize - 1);
        register_[kBlockSize - 1] = encrypting ? output : input;
    }
}

} // namespace basalt::wire::auth
