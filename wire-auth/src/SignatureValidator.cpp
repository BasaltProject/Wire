#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/auth/SignatureValidator.hpp>
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <array>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mutex>

namespace basalt::wire::auth {
namespace {

constexpr std::size_t kMaxDocumentBytes = 1024UL * 1024UL;

void putLong(std::vector<std::uint8_t>& into, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        into.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

} // namespace

struct RsaPublicKey::State {
    mbedtls_pk_context key{};
    /// mbedTLS keeps scratch state in the context, so one key is checked by one thread at a time.
    std::mutex mutex;

    State() { mbedtls_pk_init(&key); }

    ~State() { mbedtls_pk_free(&key); }

    State(const State&) = delete;
    State& operator=(const State&) = delete;
};

RsaPublicKey::RsaPublicKey() : state_(std::make_unique<State>()) {}

RsaPublicKey::RsaPublicKey(RsaPublicKey&&) noexcept = default;

RsaPublicKey& RsaPublicKey::operator=(RsaPublicKey&&) noexcept = default;

RsaPublicKey::~RsaPublicKey() = default;

std::optional<RsaPublicKey> RsaPublicKey::parseDer(std::span<const std::uint8_t> der) {
    if (der.empty()) {
        return std::nullopt;
    }
    RsaPublicKey key;
    if (mbedtls_pk_parse_public_key(&key.state_->key, der.data(), der.size()) != 0 ||
        mbedtls_pk_get_type(&key.state_->key) != MBEDTLS_PK_RSA) {
        return std::nullopt;
    }
    return key;
}

bool RsaPublicKey::verify(SignatureAlgorithm algorithm, std::span<const std::uint8_t> payload,
                          std::span<const std::uint8_t> signature) const {
    const mbedtls_md_type_t type = algorithm == SignatureAlgorithm::Sha1WithRsa ? MBEDTLS_MD_SHA1 : MBEDTLS_MD_SHA256;
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(type);
    if (info == nullptr) {
        return false;
    }
    std::array<unsigned char, 32> digest{};
    if (mbedtls_md(info, payload.data(), payload.size(), digest.data()) != 0) {
        return false;
    }
    const std::scoped_lock lock{state_->mutex};
    return mbedtls_pk_verify(&state_->key, type, digest.data(), mbedtls_md_get_size(info), signature.data(),
                             signature.size()) == 0;
}

std::vector<std::uint8_t> profileKeyPayload(const common::Uuid& profileId, std::int64_t expiresAt,
                                            std::span<const std::uint8_t> publicKeyDer) {
    std::vector<std::uint8_t> payload;
    payload.reserve(24 + publicKeyDer.size());
    putLong(payload, profileId.high());
    putLong(payload, profileId.low());
    putLong(payload, static_cast<std::uint64_t>(expiresAt));
    payload.insert(payload.end(), publicKeyDer.begin(), publicKeyDer.end());
    return payload;
}

ServicesKeySet::ServicesKeySet() = default;

ServicesKeySet::ServicesKeySet(ServicesKeySet&&) noexcept = default;

ServicesKeySet& ServicesKeySet::operator=(ServicesKeySet&&) noexcept = default;

ServicesKeySet::~ServicesKeySet() = default;

ServicesKeySet ServicesKeySet::parse(std::string_view json) {
    ServicesKeySet set;
    const std::expected<common::JsonValue, common::JsonError> parsed = common::parseJson(json);
    const auto* root = parsed.has_value() ? parsed->get<common::JsonObject>() : nullptr;
    const common::JsonValue* list = root == nullptr ? nullptr : root->find("playerCertificateKeys");
    const auto* entries = list == nullptr ? nullptr : list->get<std::vector<common::JsonValue>>();
    if (entries == nullptr) {
        return set;
    }
    for (const common::JsonValue& entry : *entries) {
        const auto* fields = entry.get<common::JsonObject>();
        const common::JsonValue* encoded = fields == nullptr ? nullptr : fields->find("publicKey");
        const auto* text = encoded == nullptr ? nullptr : encoded->get<std::string>();
        const std::optional<std::vector<std::uint8_t>> der =
            text == nullptr ? std::nullopt : common::base64::decode(*text);
        std::optional<RsaPublicKey> key = der.has_value() ? RsaPublicKey::parseDer(*der) : std::nullopt;
        if (key.has_value()) {
            set.profileKeys_.push_back(std::move(*key));
        }
    }
    return set;
}

ServicesKeySet ServicesKeySet::fetch() {
    const std::expected<HttpsResponse, HttpsError> response = httpsGet(kUrl, kMaxDocumentBytes);
    if (!response.has_value() || !response->ok()) {
        return ServicesKeySet{};
    }
    return parse(response->text());
}

ServicesKeySet ServicesKeySet::ofProfileKeys(std::vector<RsaPublicKey> keys) {
    ServicesKeySet set;
    set.profileKeys_ = std::move(keys);
    return set;
}

bool ServicesKeySet::verifyProfileKey(std::span<const std::uint8_t> payload,
                                      std::span<const std::uint8_t> signature) const {
    for (const RsaPublicKey& key : profileKeys_) {
        if (key.verify(SignatureAlgorithm::Sha1WithRsa, payload, signature)) {
            return true;
        }
    }
    return false;
}

} // namespace basalt::wire::auth
