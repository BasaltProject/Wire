#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace basalt::wire::bedrock {

using SessionKey = std::array<std::uint8_t, 32>;

/// Bedrock's packet cipher: AES-256 in counter mode with an eight byte checksum appended before encryption.
class BedrockCipher {
public:
    explicit BedrockCipher(const SessionKey& key);
    BedrockCipher(const BedrockCipher&) = delete;
    BedrockCipher& operator=(const BedrockCipher&) = delete;
    ~BedrockCipher();

    void encrypt(std::span<const std::uint8_t> payload, std::vector<std::uint8_t>& destination);

    [[nodiscard]] bool decrypt(std::span<const std::uint8_t> payload, std::vector<std::uint8_t>& destination);

private:
    struct State;

    std::unique_ptr<State> state_;
};

[[nodiscard]] SessionKey deriveSessionKey(std::span<const std::uint8_t> salt,
                                          std::span<const std::uint8_t> sharedSecret);

} // namespace basalt::wire::bedrock
