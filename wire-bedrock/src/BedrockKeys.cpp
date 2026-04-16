#include <basalt/wire/bedrock/BedrockKeys.hpp>
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <array>
#include <basalt_mbedtls_threads.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha512.h>
#include <utility>

namespace basalt::wire::bedrock {
namespace {

/// Half of a P-384 signature, and the size every coordinate on that curve is written at.
constexpr std::size_t kCoordinateSize = 48;

constexpr std::size_t kSignatureSize = kCoordinateSize * 2;

/// A DER SubjectPublicKeyInfo for P-384 is a little over a hundred bytes.
constexpr std::size_t kDerBufferSize = 256;

std::array<std::uint8_t, kCoordinateSize> sha384(std::string_view text) {
    std::array<std::uint8_t, kCoordinateSize> digest{};
    mbedtls_sha512(reinterpret_cast<const unsigned char*>(text.data()), text.size(), digest.data(), 1);
    return digest;
}

} // namespace

std::optional<JwtParts> decodeJwt(std::string_view token) {
    const std::size_t firstDot = token.find('.');
    if (firstDot == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t secondDot = token.find('.', firstDot + 1);
    if (secondDot == std::string_view::npos) {
        return std::nullopt;
    }

    const auto header = common::base64url::decode(token.substr(0, firstDot));
    const auto payload = common::base64url::decode(token.substr(firstDot + 1, secondDot - firstDot - 1));
    const auto signature = common::base64url::decode(token.substr(secondDot + 1));
    if (!header || !payload || !signature) {
        return std::nullopt;
    }

    JwtParts parts;
    parts.header.assign(header->begin(), header->end());
    parts.payload.assign(payload->begin(), payload->end());
    parts.signature = *signature;
    parts.signingInput = std::string{token.substr(0, secondDot)};
    return parts;
}

std::optional<double> jsonNumber(const std::string_view json, const std::string_view key) {
    const auto document = common::parseJson(json);
    if (!document) {
        return std::nullopt;
    }
    const auto* object = document->get<common::JsonObject>();
    if (object == nullptr) {
        return std::nullopt;
    }
    const common::JsonValue* value = object->find(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    // A whole number keeps its own type in this reader, and every count a client writes is one.
    if (const auto* whole = value->get<std::int64_t>(); whole != nullptr) {
        return static_cast<double>(*whole);
    }
    const auto* number = value->get<double>();
    return number == nullptr ? std::nullopt : std::optional{*number};
}

std::optional<std::string> jsonString(std::string_view json, std::string_view key) {
    const auto document = common::parseJson(json);
    if (!document) {
        return std::nullopt;
    }
    const auto* object = document->get<common::JsonObject>();
    if (object == nullptr) {
        return std::nullopt;
    }
    const common::JsonValue* value = object->find(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    const auto* text = value->get<std::string>();
    if (text == nullptr) {
        return std::nullopt;
    }
    return *text;
}

bool verifyJwt(const JwtParts& parts, std::span<const std::uint8_t> publicKeyDer) {
    if (parts.signature.size() != kSignatureSize || publicKeyDer.empty()) {
        return false;
    }
    const auto algorithm = jsonString(parts.header, "alg");
    if (!algorithm || *algorithm != "ES384") {
        return false;
    }

    mbedtls_pk_context key;
    basalt_mbedtls_threading_init();
    mbedtls_pk_init(&key);
    bool verified = false;

    if (mbedtls_pk_parse_public_key(&key, publicKeyDer.data(), publicKeyDer.size()) == 0 &&
        mbedtls_pk_get_type(&key) == MBEDTLS_PK_ECKEY) {
        mbedtls_ecp_group group;
        mbedtls_ecp_point point;
        mbedtls_mpi secret;
        mbedtls_mpi left;
        mbedtls_mpi right;
        basalt_mbedtls_threading_init();
        mbedtls_ecp_group_init(&group);
        mbedtls_ecp_point_init(&point);
        mbedtls_mpi_init(&secret);
        mbedtls_mpi_init(&left);
        mbedtls_mpi_init(&right);

        if (mbedtls_ecp_export(mbedtls_pk_ec(key), &group, &secret, &point) == 0 &&
            group.id == MBEDTLS_ECP_DP_SECP384R1) {
            const auto digest = sha384(parts.signingInput);
            if (mbedtls_mpi_read_binary(&left, parts.signature.data(), kCoordinateSize) == 0 &&
                mbedtls_mpi_read_binary(&right, parts.signature.data() + kCoordinateSize, kCoordinateSize) == 0) {
                verified = mbedtls_ecdsa_verify(&group, digest.data(), digest.size(), &point, &left, &right) == 0;
            }
        }

        mbedtls_mpi_free(&right);
        mbedtls_mpi_free(&left);
        mbedtls_mpi_free(&secret);
        mbedtls_ecp_point_free(&point);
        mbedtls_ecp_group_free(&group);
    }

    mbedtls_pk_free(&key);
    return verified;
}

struct EcKeyPair::State {
    mbedtls_pk_context key{};
    mbedtls_entropy_context entropy{};
    mbedtls_ctr_drbg_context random{};
    std::vector<std::uint8_t> publicDer;

    State() {
        basalt_mbedtls_threading_init();
        mbedtls_pk_init(&key);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&random);
    }

    ~State() {
        mbedtls_ctr_drbg_free(&random);
        mbedtls_entropy_free(&entropy);
        mbedtls_pk_free(&key);
    }

    State(const State&) = delete;
    State& operator=(const State&) = delete;
};

EcKeyPair::EcKeyPair() : state_(std::make_unique<State>()) {}

EcKeyPair::EcKeyPair(EcKeyPair&&) noexcept = default;

EcKeyPair& EcKeyPair::operator=(EcKeyPair&&) noexcept = default;

EcKeyPair::~EcKeyPair() = default;

std::optional<EcKeyPair> EcKeyPair::generate() {
    EcKeyPair pair;
    State& state = *pair.state_;

    const char* personalisation = "basalt-bedrock";
    if (mbedtls_ctr_drbg_seed(&state.random, mbedtls_entropy_func, &state.entropy,
                              reinterpret_cast<const unsigned char*>(personalisation),
                              std::char_traits<char>::length(personalisation)) != 0) {
        return std::nullopt;
    }
    if (mbedtls_pk_setup(&state.key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0) {
        return std::nullopt;
    }
    if (mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP384R1, mbedtls_pk_ec(state.key), mbedtls_ctr_drbg_random,
                            &state.random) != 0) {
        return std::nullopt;
    }

    std::array<std::uint8_t, kDerBufferSize> buffer{};
    const int written = mbedtls_pk_write_pubkey_der(&state.key, buffer.data(), buffer.size());
    if (written <= 0) {
        return std::nullopt;
    }
    // mbedTLS writes DER backwards, so the bytes end at the tail of the buffer.
    const auto* start = buffer.data() + buffer.size() - static_cast<std::size_t>(written);
    state.publicDer.assign(start, start + written);
    return pair;
}

std::span<const std::uint8_t> EcKeyPair::publicKeyDer() const noexcept { return state_->publicDer; }

std::string EcKeyPair::publicKeyBase64() const { return common::base64::encode(state_->publicDer); }

std::optional<std::vector<std::uint8_t>> EcKeyPair::sharedSecret(std::span<const std::uint8_t> peerKeyDer) const {
    if (peerKeyDer.empty()) {
        return std::nullopt;
    }

    mbedtls_pk_context peer;
    basalt_mbedtls_threading_init();
    mbedtls_pk_init(&peer);
    std::optional<std::vector<std::uint8_t>> result;

    if (mbedtls_pk_parse_public_key(&peer, peerKeyDer.data(), peerKeyDer.size()) == 0 &&
        mbedtls_pk_get_type(&peer) == MBEDTLS_PK_ECKEY) {
        mbedtls_ecp_group group;
        mbedtls_ecp_group peerGroup;
        mbedtls_ecp_point ours;
        mbedtls_ecp_point theirs;
        mbedtls_mpi ourSecret;
        mbedtls_mpi theirSecret;
        mbedtls_mpi shared;
        basalt_mbedtls_threading_init();
        mbedtls_ecp_group_init(&group);
        mbedtls_ecp_group_init(&peerGroup);
        mbedtls_ecp_point_init(&ours);
        mbedtls_ecp_point_init(&theirs);
        mbedtls_mpi_init(&ourSecret);
        mbedtls_mpi_init(&theirSecret);
        mbedtls_mpi_init(&shared);

        if (mbedtls_ecp_export(mbedtls_pk_ec(state_->key), &group, &ourSecret, &ours) == 0 &&
            mbedtls_ecp_export(mbedtls_pk_ec(peer), &peerGroup, &theirSecret, &theirs) == 0 &&
            peerGroup.id == MBEDTLS_ECP_DP_SECP384R1 &&
            mbedtls_ecdh_compute_shared(&group, &shared, &theirs, &ourSecret, mbedtls_ctr_drbg_random,
                                        &state_->random) == 0) {
            std::vector<std::uint8_t> bytes(kCoordinateSize);
            if (mbedtls_mpi_write_binary(&shared, bytes.data(), bytes.size()) == 0) {
                result = std::move(bytes);
            }
        }

        mbedtls_mpi_free(&shared);
        mbedtls_mpi_free(&theirSecret);
        mbedtls_mpi_free(&ourSecret);
        mbedtls_ecp_point_free(&theirs);
        mbedtls_ecp_point_free(&ours);
        mbedtls_ecp_group_free(&peerGroup);
        mbedtls_ecp_group_free(&group);
    }

    mbedtls_pk_free(&peer);
    return result;
}

std::optional<std::string> EcKeyPair::signJwt(std::string_view payloadJson) const {
    const std::string header = R"({"alg":"ES384","x5u":")" + publicKeyBase64() + R"("})";
    const std::string signingInput =
        common::base64url::encode({reinterpret_cast<const std::uint8_t*>(header.data()), header.size()}) + '.' +
        common::base64url::encode({reinterpret_cast<const std::uint8_t*>(payloadJson.data()), payloadJson.size()});

    mbedtls_ecp_group group;
    mbedtls_ecp_point point;
    mbedtls_mpi secret;
    mbedtls_mpi left;
    mbedtls_mpi right;
    basalt_mbedtls_threading_init();
    mbedtls_ecp_group_init(&group);
    mbedtls_ecp_point_init(&point);
    mbedtls_mpi_init(&secret);
    mbedtls_mpi_init(&left);
    mbedtls_mpi_init(&right);

    std::optional<std::string> token;
    const auto digest = sha384(signingInput);
    if (mbedtls_ecp_export(mbedtls_pk_ec(state_->key), &group, &secret, &point) == 0 &&
        mbedtls_ecdsa_sign(&group, &left, &right, &secret, digest.data(), digest.size(), mbedtls_ctr_drbg_random,
                           &state_->random) == 0) {
        std::vector<std::uint8_t> signature(kSignatureSize);
        if (mbedtls_mpi_write_binary(&left, signature.data(), kCoordinateSize) == 0 &&
            mbedtls_mpi_write_binary(&right, signature.data() + kCoordinateSize, kCoordinateSize) == 0) {
            token = signingInput + '.' + common::base64url::encode(signature);
        }
    }

    mbedtls_mpi_free(&right);
    mbedtls_mpi_free(&left);
    mbedtls_mpi_free(&secret);
    mbedtls_ecp_point_free(&point);
    mbedtls_ecp_group_free(&group);
    return token;
}

} // namespace basalt::wire::bedrock
