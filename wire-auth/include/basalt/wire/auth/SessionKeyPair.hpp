#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::auth {

/// parity: Crypt.generateKeyPair, 1024-bit RSA with PKCS#1 v1.5 padding; public key as X.509 DER.
class SessionKeyPair {
public:
    SessionKeyPair(const SessionKeyPair&) = delete;
    SessionKeyPair& operator=(const SessionKeyPair&) = delete;
    SessionKeyPair(SessionKeyPair&&) noexcept;
    SessionKeyPair& operator=(SessionKeyPair&&) noexcept;
    ~SessionKeyPair();

    [[nodiscard]] static std::optional<SessionKeyPair> generate();

    [[nodiscard]] std::span<const std::uint8_t> publicKeyDer() const noexcept;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> decrypt(std::span<const std::uint8_t> ciphertext) const;

private:
    struct State;

    SessionKeyPair();

    std::unique_ptr<State> state_;
};

/// parity: Crypt.digestData: SHA-1 over server id (ISO-8859-1), shared secret and DER key, as BigInteger hex.
[[nodiscard]] std::string sessionHash(std::string_view serverId, std::span<const std::uint8_t> secretKey,
                                      std::span<const std::uint8_t> publicKeyDer);

/// Signed two's-complement hex with no leading zeros and a minus sign when negative, like BigInteger.
[[nodiscard]] std::string formatSessionHash(std::span<const std::uint8_t> digest);

[[nodiscard]] bool randomBytes(std::span<std::uint8_t> into);

} // namespace basalt::wire::auth
