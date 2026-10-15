#pragma once

#include <basalt/wire/auth/SessionKeyPair.hpp>

#include <array>
#include <mbedtls/pk.h>
#include <span>
#include <stdexcept>
#include <vector>

namespace basalt::wire::auth::test {

inline std::vector<std::uint8_t> encryptRsa(std::span<const std::uint8_t> der, std::span<const std::uint8_t> plain) {
    struct Key {
        mbedtls_pk_context value{};

        Key() { mbedtls_pk_init(&value); }

        ~Key() { mbedtls_pk_free(&value); }
    } key;

    if (mbedtls_pk_parse_public_key(&key.value, der.data(), der.size()) != 0)
        throw std::runtime_error("synthetic client public key parse failed");
    std::array<std::uint8_t, 512> output{};
    std::size_t written = 0;
    const auto random = [](void*, unsigned char* data, std::size_t size) {
        return randomBytes(std::span{data, size}) ? 0 : -1;
    };
    if (mbedtls_pk_encrypt(&key.value, plain.data(), plain.size(), output.data(), &written, output.size(), random,
                           nullptr) != 0)
        throw std::runtime_error("synthetic client RSA encryption failed");
    return {output.begin(), output.begin() + static_cast<std::ptrdiff_t>(written)};
}

} // namespace basalt::wire::auth::test
