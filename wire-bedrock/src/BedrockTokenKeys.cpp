#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/bedrock/BedrockKeys.hpp>
#include <basalt/wire/bedrock/BedrockTokenKeys.hpp>
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <basalt_mbedtls_threads.h>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mutex>
#include <stop_token>
#include <thread>

namespace basalt::wire::bedrock {
namespace {

constexpr std::string_view kDiscoveryUrl =
    "https://client.discovery.minecraft-services.net/api/v1.0/discovery/MinecraftPE/builds/1.0.0.0";
constexpr std::string_view kAudience = "api://auth-minecraft-services/multiplayer";
constexpr std::size_t kMaxBytes = 256UL * 1024UL;
constexpr auto kRequestTimeout = std::chrono::seconds(10);
constexpr auto kRefreshEvery = std::chrono::seconds(6 * 3600);
constexpr auto kFirstRetry = std::chrono::seconds(15);
constexpr auto kLastRetry = std::chrono::seconds(600);
constexpr auto kUnknownKeyRefreshGap = std::chrono::seconds(60);

const common::JsonValue* member(const common::JsonValue* value, std::string_view name) {
    const auto* object = value == nullptr ? nullptr : value->get<common::JsonObject>();
    return object == nullptr ? nullptr : object->find(name);
}

std::string text(const common::JsonValue* value) {
    const auto* string = value == nullptr ? nullptr : value->get<std::string>();
    return string == nullptr ? std::string{} : *string;
}

void appendDer(std::vector<std::uint8_t>& out, std::uint8_t tag, std::span<const std::uint8_t> body) {
    out.push_back(tag);
    if (body.size() < 0x80) {
        out.push_back(static_cast<std::uint8_t>(body.size()));
    } else if (body.size() < 0x100) {
        out.push_back(0x81);
        out.push_back(static_cast<std::uint8_t>(body.size()));
    } else {
        out.push_back(0x82);
        out.push_back(static_cast<std::uint8_t>(body.size() >> 8));
        out.push_back(static_cast<std::uint8_t>(body.size() & 0xff));
    }
    out.insert(out.end(), body.begin(), body.end());
}

void appendDerInteger(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> magnitude) {
    while (magnitude.size() > 1 && magnitude.front() == 0) {
        magnitude = magnitude.subspan(1);
    }
    std::vector<std::uint8_t> body;
    if (magnitude.empty() || (magnitude.front() & 0x80) != 0) {
        body.push_back(0);
    }
    body.insert(body.end(), magnitude.begin(), magnitude.end());
    appendDer(out, 0x02, body);
}

std::optional<std::vector<std::uint8_t>> rsaPublicKeyDer(const std::string& modulus, const std::string& exponent) {
    const auto n = common::base64url::decode(modulus);
    const auto e = common::base64url::decode(exponent);
    if (!n || !e || n->size() < 128 || n->size() > 1024 || e->empty() || e->size() > 8) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> numbers;
    appendDerInteger(numbers, *n);
    appendDerInteger(numbers, *e);
    std::vector<std::uint8_t> rsaKey;
    appendDer(rsaKey, 0x30, numbers);
    std::vector<std::uint8_t> bits{0};
    bits.insert(bits.end(), rsaKey.begin(), rsaKey.end());
    static constexpr std::array<std::uint8_t, 15> kAlgorithm = {0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
                                                                0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00};
    std::vector<std::uint8_t> info(kAlgorithm.begin(), kAlgorithm.end());
    appendDer(info, 0x03, bits);
    std::vector<std::uint8_t> der;
    appendDer(der, 0x30, info);
    return der;
}

bool verifyRs256(const JwtParts& token, std::span<const std::uint8_t> publicKeyDer) {
    if (jsonString(token.header, "alg") != std::optional<std::string>{"RS256"} || token.signature.empty()) {
        return false;
    }
    std::array<std::uint8_t, 32> digest{};
    if (mbedtls_sha256(reinterpret_cast<const unsigned char*>(token.signingInput.data()), token.signingInput.size(),
                       digest.data(), 0) != 0) {
        return false;
    }
    basalt_mbedtls_threading_init();
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    const bool verified = mbedtls_pk_parse_public_key(&key, publicKeyDer.data(), publicKeyDer.size()) == 0 &&
                          mbedtls_pk_get_type(&key) == MBEDTLS_PK_RSA &&
                          mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, digest.data(), digest.size(),
                                            token.signature.data(), token.signature.size()) == 0;
    mbedtls_pk_free(&key);
    return verified;
}

std::optional<common::JsonValue> parse(const std::optional<std::string>& body) {
    if (!body) {
        return std::nullopt;
    }
    auto document = common::parseJson(*body);
    return document ? std::optional<common::JsonValue>{std::move(*document)} : std::nullopt;
}

std::optional<std::string> fetch(std::string_view url) {
    const auth::BoundedHttpsOptions options{.deadline = std::chrono::steady_clock::now() + kRequestTimeout};
    const auto response = auth::httpsGetBounded(url, kMaxBytes, options);
    if (!response.has_value() || !response->ok()) {
        return std::nullopt;
    }
    return response->text();
}

} // namespace

struct MojangTokenKeys::State {
    std::mutex mutex;
    std::string issuer;
    std::map<std::string, std::vector<std::uint8_t>, std::less<>> keys;
    std::chrono::steady_clock::time_point lastUnknownKeyRefresh{};

    std::atomic<bool> started{false};
    std::mutex wakeMutex;
    std::condition_variable_any wake;
    bool refreshWanted = false;
    std::jthread worker;

    bool load() {
        const auto discovery = fetch(kDiscoveryUrl);
        const auto document = parse(discovery);
        if (!document) {
            return false;
        }
        const common::JsonValue* prod = member(member(member(&*document, "result"), "serviceEnvironments"), "auth");
        const std::string base = text(member(member(prod, "prod"), "serviceUri"));
        if (base.empty() || !base.starts_with("https://")) {
            return false;
        }
        const auto openId = fetch(base + "/.well-known/openid-configuration");
        const auto configuration = parse(openId);
        if (!configuration) {
            return false;
        }
        const std::string newIssuer = text(member(&*configuration, "issuer"));
        const std::string jwksUri = text(member(&*configuration, "jwks_uri"));
        if (newIssuer.empty() || !jwksUri.starts_with("https://")) {
            return false;
        }
        const auto jwksText = fetch(jwksUri);
        const auto jwks = parse(jwksText);
        const auto* list = jwks ? member(&*jwks, "keys") : nullptr;
        const auto* entries = list == nullptr ? nullptr : list->get<std::vector<common::JsonValue>>();
        if (entries == nullptr) {
            return false;
        }
        std::vector<std::pair<std::string, std::vector<std::uint8_t>>> found;
        for (const common::JsonValue& entry : *entries) {
            const std::string kid = text(member(&entry, "kid"));
            if (text(member(&entry, "kty")) != "RSA") {
                continue;
            }
            if (auto der = rsaPublicKeyDer(text(member(&entry, "n")), text(member(&entry, "e"))); !kid.empty() && der) {
                found.emplace_back(kid, std::move(*der));
            }
        }
        if (found.empty()) {
            return false;
        }
        install(newIssuer, std::move(found));
        return true;
    }

    void install(std::string newIssuer, std::vector<std::pair<std::string, std::vector<std::uint8_t>>> found) {
        const std::scoped_lock lock{mutex};
        issuer = std::move(newIssuer);
        keys.clear();
        for (auto& [kid, der] : found) {
            keys.emplace(std::move(kid), std::move(der));
        }
    }

    void run(const std::stop_token stop) {
        std::chrono::seconds delay = kFirstRetry;
        while (!stop.stop_requested()) {
            std::chrono::seconds pause;
            if (load()) {
                delay = kFirstRetry;
                pause = kRefreshEvery;
            } else {
                pause = delay;
                delay = std::min<std::chrono::seconds>(delay * 2, kLastRetry);
            }
            std::unique_lock lock{wakeMutex};
            wake.wait_for(lock, stop, pause, [this] { return refreshWanted; });
            refreshWanted = false;
        }
    }
};

MojangTokenKeys::MojangTokenKeys() : state_(std::make_unique<State>()) {}

MojangTokenKeys::~MojangTokenKeys() = default;

MojangTokenKeys& MojangTokenKeys::instance() {
    static MojangTokenKeys keys;
    return keys;
}

void MojangTokenKeys::start() {
    if (state_->started.exchange(true)) {
        return;
    }
    state_->worker = std::jthread([state = state_.get()](const std::stop_token stop) { state->run(stop); });
}

void MojangTokenKeys::install(std::string issuer, std::vector<std::pair<std::string, std::vector<std::uint8_t>>> keys) {
    state_->install(std::move(issuer), std::move(keys));
}

bool MojangTokenKeys::verify(const JwtParts& token) {
    const auto kid = jsonString(token.header, "kid");
    std::vector<std::uint8_t> key;
    std::string issuer;
    bool unknownKey = false;
    {
        const std::scoped_lock lock{state_->mutex};
        issuer = state_->issuer;
        if (kid) {
            if (const auto found = state_->keys.find(*kid); found != state_->keys.end()) {
                key = found->second;
            } else if (!state_->keys.empty() &&
                       std::chrono::steady_clock::now() - state_->lastUnknownKeyRefresh > kUnknownKeyRefreshGap) {
                state_->lastUnknownKeyRefresh = std::chrono::steady_clock::now();
                unknownKey = true;
            }
        }
    }
    if (unknownKey) {
        {
            const std::scoped_lock lock{state_->wakeMutex};
            state_->refreshWanted = true;
        }
        state_->wake.notify_all();
    }
    if (key.empty() || issuer.empty() || !verifyRs256(token, key)) {
        return false;
    }
    const auto now = static_cast<double>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    const auto expires = jsonNumber(token.payload, "exp");
    const auto notBefore = jsonNumber(token.payload, "nbf");
    return expires && *expires > now && (!notBefore || *notBefore <= now) &&
           jsonString(token.payload, "iss") == std::optional<std::string>{issuer} &&
           jsonString(token.payload, "aud") == std::optional<std::string>{std::string{kAudience}};
}

} // namespace basalt::wire::bedrock
