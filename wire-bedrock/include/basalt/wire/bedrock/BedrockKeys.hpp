#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::bedrock {

struct JwtParts {
    std::string header;
    std::string payload;
    std::vector<std::uint8_t> signature;
    std::string signingInput;
};

[[nodiscard]] std::optional<JwtParts> decodeJwt(std::string_view token);

[[nodiscard]] std::optional<std::string> jsonString(std::string_view json, std::string_view key);

[[nodiscard]] std::optional<double> jsonNumber(std::string_view json, std::string_view key);

[[nodiscard]] bool verifyJwt(const JwtParts& parts, std::span<const std::uint8_t> publicKeyDer);

/// The server makes one per run and the handshake never sends the private half anywhere.
class EcKeyPair {
public:
    EcKeyPair();
    EcKeyPair(const EcKeyPair&) = delete;
    EcKeyPair& operator=(const EcKeyPair&) = delete;
    EcKeyPair(EcKeyPair&&) noexcept;
    EcKeyPair& operator=(EcKeyPair&&) noexcept;
    ~EcKeyPair();

    [[nodiscard]] static std::optional<EcKeyPair> generate();

    [[nodiscard]] std::span<const std::uint8_t> publicKeyDer() const noexcept;

    [[nodiscard]] std::string publicKeyBase64() const;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> sharedSecret(std::span<const std::uint8_t> peerKeyDer) const;

    [[nodiscard]] std::optional<std::string> signJwt(std::string_view payloadJson) const;

private:
    struct State;

    std::unique_ptr<State> state_;
};

} // namespace basalt::wire::bedrock
