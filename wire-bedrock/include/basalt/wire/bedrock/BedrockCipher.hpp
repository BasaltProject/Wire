#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace basalt::wire::bedrock {

/// The session key both sides derive once the handshake is done: 32 bytes, which makes it AES-256.
using SessionKey = std::array<std::uint8_t, 32>;

/// Bedrock's packet cipher: AES-256 in counter mode with an eight byte checksum appended before
/// encryption. The checksum covers a counter only the two ends know, so a datagram cannot be
/// replayed or reordered unnoticed. Each direction has its own counter and keystream.
class BedrockCipher {
public:
    explicit BedrockCipher(const SessionKey& key);
    BedrockCipher(const BedrockCipher&) = delete;
    BedrockCipher& operator=(const BedrockCipher&) = delete;
    ~BedrockCipher();

    /// Appends the checksum and encrypts, leaving the result in destination.
    void encrypt(std::span<const std::uint8_t> payload, std::vector<std::uint8_t>& destination);

    /// Decrypts and checks the trailer. False when the checksum did not match, which is a session
    /// that has to end: a wrong one means the stream is out of step and nothing after it is
    /// readable either.
    [[nodiscard]] bool decrypt(std::span<const std::uint8_t> payload, std::vector<std::uint8_t>& destination);

private:
    struct State;

    std::unique_ptr<State> state_;
};

/// SHA-256 of the salt followed by the shared secret, which is how both ends arrive at the same key
/// without either sending it.
[[nodiscard]] SessionKey deriveSessionKey(std::span<const std::uint8_t> salt,
                                          std::span<const std::uint8_t> sharedSecret);

} // namespace basalt::wire::bedrock
