#include <basalt/wire/bedrock/NetherNetIdentity.hpp>
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <chrono>

namespace basalt::wire::bedrock {
namespace {

std::string base64urlOf(std::string_view text) {
    return common::base64url::encode({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

std::string jsonQuoted(std::string_view text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    return out + '"';
}

std::optional<std::vector<std::uint8_t>> claimedPublicKey(std::string_view payloadJson) {
    const auto document = common::parseJson(payloadJson);
    if (!document) {
        return std::nullopt;
    }
    const auto* claims = document->get<common::JsonObject>();
    if (claims == nullptr) {
        return std::nullopt;
    }
    const common::JsonValue* cpk = claims->find("cpk");
    if (cpk == nullptr) {
        return std::nullopt;
    }
    if (const auto* text = cpk->get<std::string>(); text != nullptr) {
        return common::base64::decode(*text);
    }
    const auto* key = cpk->get<common::JsonObject>();
    if (key == nullptr) {
        return std::nullopt;
    }
    std::string jwk = "{";
    for (const std::string_view field : {"kty", "crv", "x", "y"}) {
        const common::JsonValue* value = key->find(field);
        const auto* text = value == nullptr ? nullptr : value->get<std::string>();
        if (text == nullptr) {
            return std::nullopt;
        }
        jwk += (jwk.size() > 1 ? "," : "") + jsonQuoted(field) + ":" + jsonQuoted(*text);
    }
    return publicKeyDerFromJwk(jwk + "}");
}

} // namespace

std::string netherNetFingerprintJson(std::string_view algorithm, std::string_view digest) {
    return R"({"fingerprint":[{"algorithm":)" + jsonQuoted(algorithm) + R"(,"digest":)" + jsonQuoted(digest) + "}]}";
}

std::expected<NetherNetPeerIdentity, std::string>
verifyNetherNetOfferIdentity(std::string_view identityBase64, std::string_view algorithm, std::string_view digest) {
    const auto envelopeBytes = common::base64::decode(identityBase64);
    if (!envelopeBytes) {
        return std::unexpected{"the identity is not base64"};
    }
    const std::string envelope(envelopeBytes->begin(), envelopeBytes->end());
    const auto document = common::parseJson(envelope);
    const auto* root = document ? document->get<common::JsonObject>() : nullptr;
    if (root == nullptr) {
        return std::unexpected{"the identity is not JSON"};
    }

    NetherNetPeerIdentity identity;
    const common::JsonValue* provider = root->find("idp");
    const auto* providerObject = provider == nullptr ? nullptr : provider->get<common::JsonObject>();
    if (providerObject == nullptr) {
        return std::unexpected{"the identity has no provider"};
    }
    if (const common::JsonValue* domain = providerObject->find("domain"); domain != nullptr) {
        if (const auto* text = domain->get<std::string>(); text != nullptr) {
            identity.domain = *text;
        }
    }

    const auto assertion = jsonString(envelope, "assertion");
    const auto token = assertion ? jsonString(*assertion, "token") : std::nullopt;
    const auto fingerprints = assertion ? jsonString(*assertion, "fingerprints") : std::nullopt;
    if (!token || !fingerprints) {
        return std::unexpected{"the identity assertion is incomplete"};
    }

    const auto tokenParts = decodeJwt(*token);
    if (!tokenParts) {
        return std::unexpected{"the identity token is malformed"};
    }
    const auto key = claimedPublicKey(tokenParts->payload);
    if (!key) {
        return std::unexpected{"the identity token carries no key"};
    }
    identity.publicKeyDer = *key;
    identity.selfSigned = verifyJwt(*tokenParts, identity.publicKeyDer);

    // A detached JWS is header..signature; the payload is rebuilt from the offer itself.
    const std::size_t gap = fingerprints->find("..");
    if (gap == std::string::npos) {
        return std::unexpected{"the fingerprint signature is not detached"};
    }
    const std::string signingInput =
        fingerprints->substr(0, gap) + '.' + base64urlOf(netherNetFingerprintJson(algorithm, digest));
    const auto signature = common::base64url::decode(fingerprints->substr(gap + 2));
    if (!signature || !verifyEs384(signingInput, *signature, identity.publicKeyDer)) {
        return std::unexpected{"the fingerprint signature does not match the offer"};
    }
    return identity;
}

std::optional<std::string> buildNetherNetAnswerIdentity(const EcKeyPair& key, std::string_view domain,
                                                        std::string_view digest) {
    const auto issued =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    constexpr std::int64_t kLifetimeSeconds = 24 * 60 * 60;
    const std::string claims = R"({"cpk":)" + key.publicKeyJwk() + R"(,"exp":)" +
                               std::to_string(issued + kLifetimeSeconds) + R"(,"iat":)" + std::to_string(issued) + "}";
    const auto token = key.signJwt(claims);
    if (!token) {
        return std::nullopt;
    }

    const std::string header = base64urlOf(R"({"alg":"ES384"})");
    const std::string signingInput = header + '.' + base64urlOf(netherNetFingerprintJson("sha-256", digest));
    const auto signature = key.sign(signingInput);
    if (!signature) {
        return std::nullopt;
    }
    const std::string detached = header + ".." + common::base64url::encode(*signature);

    const std::string assertion =
        R"({"fingerprints":)" + jsonQuoted(detached) + R"(,"token":)" + jsonQuoted(*token) + "}";
    const std::string envelope = R"({"assertion":)" + jsonQuoted(assertion) + R"(,"idp":{"domain":)" +
                                 jsonQuoted(domain) + R"(,"protocol":"default"}})";
    return common::base64::encode({reinterpret_cast<const std::uint8_t*>(envelope.data()), envelope.size()});
}

} // namespace basalt::wire::bedrock
