#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::bedrock {

/// A compact JWS taken apart: the two JSON halves decoded, the signature raw, and the exact text
/// the signature covers. Nothing here is checked yet, which is why the signing input is kept.
struct JwtParts {
    std::string header;
    std::string payload;
    std::vector<std::uint8_t> signature;
    std::string signingInput;
};

[[nodiscard]] std::optional<JwtParts> decodeJwt(std::string_view token);

/// Reads one string member out of a JSON object, which is all the login chain ever asks for.
[[nodiscard]] std::optional<std::string> jsonString(std::string_view json, std::string_view key);

/// The same for a number, which a skin's own size arrives as.
[[nodiscard]] std::optional<double> jsonNumber(std::string_view json, std::string_view key);

/// True when the token was signed by the key in that SubjectPublicKeyInfo. ES384 only: it is the
/// only algorithm a Bedrock identity is ever signed with, and accepting another is how a chain gets
/// forged.
[[nodiscard]] bool verifyJwt(const JwtParts& parts, std::span<const std::uint8_t> publicKeyDer);

/// A key pair on secp384r1, the curve every Bedrock identity uses. The server makes one per run and
/// the handshake never sends the private half anywhere.
class EcKeyPair {
public:
    EcKeyPair();
    EcKeyPair(const EcKeyPair&) = delete;
    EcKeyPair& operator=(const EcKeyPair&) = delete;
    EcKeyPair(EcKeyPair&&) noexcept;
    EcKeyPair& operator=(EcKeyPair&&) noexcept;
    ~EcKeyPair();

    [[nodiscard]] static std::optional<EcKeyPair> generate();

    /// The public half as a SubjectPublicKeyInfo, which is the shape the chain carries it in.
    [[nodiscard]] std::span<const std::uint8_t> publicKeyDer() const noexcept;

    [[nodiscard]] std::string publicKeyBase64() const;

    /// The X coordinate of the shared point, 48 bytes, which is what both ends hash into the
    /// session key.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> sharedSecret(std::span<const std::uint8_t> peerKeyDer) const;

    /// Signs the two halves into a compact JWS with ES384, putting this key in the x5u header where
    /// a Bedrock client looks for it.
    [[nodiscard]] std::optional<std::string> signJwt(std::string_view payloadJson) const;

private:
    struct State;

    std::unique_ptr<State> state_;
};

} // namespace basalt::wire::bedrock
