#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::auth {

/// The server's RSA keypair for the online-mode login handshake.
///
/// parity: net.minecraft.util.Crypt.generateKeyPair, 1024-bit RSA with PKCS#1 v1.5 padding for
/// encryption (`Cipher.getInstance("RSA")` defaults to `RSA/ECB/PKCS1Padding`), and the public key
/// sent to the client as an X.509 SubjectPublicKeyInfo DER blob, which is what `getEncoded` returns.
class SessionKeyPair {
public:
    SessionKeyPair(const SessionKeyPair&) = delete;
    SessionKeyPair& operator=(const SessionKeyPair&) = delete;
    SessionKeyPair(SessionKeyPair&&) noexcept;
    SessionKeyPair& operator=(SessionKeyPair&&) noexcept;
    ~SessionKeyPair();

    [[nodiscard]] static std::optional<SessionKeyPair> generate();

    /// X.509 SubjectPublicKeyInfo DER, sent to the client in ClientboundHelloPacket.
    [[nodiscard]] std::span<const std::uint8_t> publicKeyDer() const noexcept;

    /// Decrypts an RSA-PKCS1 payload with the private key: the shared secret, or the challenge.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> decrypt(std::span<const std::uint8_t> ciphertext) const;

private:
    struct State;

    SessionKeyPair();

    std::unique_ptr<State> state_;
};

/// parity: Crypt.digestData(serverId, publicKey, secretKey). SHA-1 over the ISO-8859-1 bytes of the
/// server id, then the shared secret, then the DER public key, formatted the way vanilla's
/// `new BigInteger(digest).toString(16)` does.
[[nodiscard]] std::string sessionHash(std::string_view serverId, std::span<const std::uint8_t> secretKey,
                                      std::span<const std::uint8_t> publicKeyDer);

/// The digest formatted as a signed two's-complement big integer in lowercase hex, with no leading
/// zeros and a minus sign when negative. Split out because it is the part that is easy to get
/// subtly wrong.
[[nodiscard]] std::string formatSessionHash(std::span<const std::uint8_t> digest);

/// Fills the span with cryptographically random bytes: the login challenge and, on the client side,
/// the shared secret.
[[nodiscard]] bool randomBytes(std::span<std::uint8_t> into);

} // namespace basalt::wire::auth
