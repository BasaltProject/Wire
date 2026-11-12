#pragma once

#include <basalt/wire/bedrock/BedrockKeys.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::bedrock {

/// Who a NetherNet offer proved it came from: the key that signed its DTLS fingerprint.
struct NetherNetPeerIdentity {
    std::vector<std::uint8_t> publicKeyDer;
    std::string domain;
    /// True when the token was signed by the key it carries, as a client without an account does.
    bool selfSigned = false;
};

/// The exact bytes both sides sign: keys sorted, no whitespace.
[[nodiscard]] std::string netherNetFingerprintJson(std::string_view algorithm, std::string_view digest);

[[nodiscard]] std::expected<NetherNetPeerIdentity, std::string>
verifyNetherNetOfferIdentity(std::string_view identityBase64, std::string_view algorithm, std::string_view digest);

/// The `a=identity` value for an answer: a token naming the server key, and its signature over the fingerprint.
[[nodiscard]] std::optional<std::string> buildNetherNetAnswerIdentity(const EcKeyPair& key, std::string_view domain,
                                                                      std::string_view digest);

} // namespace basalt::wire::bedrock
