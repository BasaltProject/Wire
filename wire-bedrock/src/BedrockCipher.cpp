#include <basalt/wire/bedrock/BedrockCipher.hpp>

#include <cstring>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>

namespace basalt::wire::bedrock {
namespace {

constexpr std::size_t kTrailerSize = 8;

/// One direction of the stream: its own key schedule, its own counter block, and the packet number
/// the checksum is built on.
struct Direction {
    mbedtls_aes_context aes{};
    std::array<std::uint8_t, 16> counterBlock{};
    std::array<std::uint8_t, 16> streamBlock{};
    std::size_t offset = 0;
    std::uint64_t packets = 0;
};

void initDirection(Direction& direction, const SessionKey& key) {
    mbedtls_aes_init(&direction.aes);
    mbedtls_aes_setkey_enc(&direction.aes, key.data(), 256);
    // The counter is the first twelve bytes of the key and then a two, which is what Bedrock does
    // with what would be a GCM nonce.
    std::memcpy(direction.counterBlock.data(), key.data(), 12);
    direction.counterBlock[15] = 2;
}

void crypt(Direction& direction, std::span<const std::uint8_t> input, std::uint8_t* output) {
    mbedtls_aes_crypt_ctr(&direction.aes, input.size(), &direction.offset, direction.counterBlock.data(),
                          direction.streamBlock.data(), input.data(), output);
}

/// SHA-256 over the packet counter, the payload and the key; the first eight bytes of it travel.
std::array<std::uint8_t, kTrailerSize> trailer(std::uint64_t counter, std::span<const std::uint8_t> payload,
                                               const SessionKey& key) {
    std::array<std::uint8_t, 8> counterBytes{};
    for (std::size_t index = 0; index < 8; ++index) {
        counterBytes[index] = static_cast<std::uint8_t>((counter >> (index * 8)) & 0xffU);
    }

    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    mbedtls_sha256_starts(&context, 0);
    mbedtls_sha256_update(&context, counterBytes.data(), counterBytes.size());
    if (!payload.empty()) {
        mbedtls_sha256_update(&context, payload.data(), payload.size());
    }
    mbedtls_sha256_update(&context, key.data(), key.size());

    std::array<std::uint8_t, 32> digest{};
    mbedtls_sha256_finish(&context, digest.data());
    mbedtls_sha256_free(&context);

    std::array<std::uint8_t, kTrailerSize> result{};
    std::memcpy(result.data(), digest.data(), kTrailerSize);
    return result;
}

} // namespace

struct BedrockCipher::State {
    SessionKey key{};
    Direction outbound;
    Direction inbound;
    std::vector<std::uint8_t> scratch;
};

BedrockCipher::BedrockCipher(const SessionKey& key) : state_(std::make_unique<State>()) {
    state_->key = key;
    initDirection(state_->outbound, key);
    initDirection(state_->inbound, key);
}

BedrockCipher::~BedrockCipher() {
    mbedtls_aes_free(&state_->outbound.aes);
    mbedtls_aes_free(&state_->inbound.aes);
}

void BedrockCipher::encrypt(std::span<const std::uint8_t> payload, std::vector<std::uint8_t>& destination) {
    const auto check = trailer(state_->outbound.packets++, payload, state_->key);

    state_->scratch.clear();
    state_->scratch.insert(state_->scratch.end(), payload.begin(), payload.end());
    state_->scratch.insert(state_->scratch.end(), check.begin(), check.end());

    destination.resize(state_->scratch.size());
    crypt(state_->outbound, state_->scratch, destination.data());
}

bool BedrockCipher::decrypt(std::span<const std::uint8_t> payload, std::vector<std::uint8_t>& destination) {
    if (payload.size() < kTrailerSize) {
        return false;
    }

    destination.resize(payload.size());
    crypt(state_->inbound, payload, destination.data());

    const std::size_t bodySize = destination.size() - kTrailerSize;
    const auto expected = trailer(state_->inbound.packets++, std::span{destination}.first(bodySize), state_->key);
    if (std::memcmp(expected.data(), destination.data() + bodySize, kTrailerSize) != 0) {
        return false;
    }
    destination.resize(bodySize);
    return true;
}

SessionKey deriveSessionKey(std::span<const std::uint8_t> salt, std::span<const std::uint8_t> sharedSecret) {
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    mbedtls_sha256_starts(&context, 0);
    if (!salt.empty()) {
        mbedtls_sha256_update(&context, salt.data(), salt.size());
    }
    if (!sharedSecret.empty()) {
        mbedtls_sha256_update(&context, sharedSecret.data(), sharedSecret.size());
    }
    SessionKey key{};
    mbedtls_sha256_finish(&context, key.data());
    mbedtls_sha256_free(&context);
    return key;
}

} // namespace basalt::wire::bedrock
