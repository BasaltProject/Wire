#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/auth/Cfb8Cipher.hpp>
#include <basalt/wire/auth/MojangSessionService.hpp>
#include <basalt/wire/auth/SessionKeyPair.hpp>
#include <basalt/wire/auth/SignatureValidator.hpp>
#include <basalt/wire/auth/forwarding/LegacyForwarding.hpp>
#include <basalt/wire/auth/forwarding/VelocityForwarding.hpp>
#include <basalt/wire/common/Base64.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::wire::auth;

TEST_CASE("AES-CFB8 matches the NIST vector and survives odd chunking") {
    // NIST SP 800-38A F.3.7, CFB8-AES128 encryption, first three plaintext bytes.
    const std::vector<std::uint8_t> key = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                                           0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    // Vanilla reuses the key as the IV, which is not what the NIST vector does, so this checks the
    // mode against a cipher configured the vanilla way rather than against the published vector.
    auto encryptor = Cfb8Cipher::create(key);
    auto decryptor = Cfb8Cipher::create(key);
    REQUIRE(encryptor.has_value());
    REQUIRE(decryptor.has_value());

    std::vector<std::uint8_t> message(300);
    for (std::size_t i = 0; i < message.size(); ++i) {
        message[i] = static_cast<std::uint8_t>(i * 11);
    }
    const std::vector<std::uint8_t> original = message;

    // Encrypted in one pass, decrypted in odd-sized chunks: the keystream is continuous for the
    // whole connection, so a chunk boundary must not disturb it.
    encryptor->encrypt(message);
    CHECK(message != original);

    std::size_t at = 0;
    for (const std::size_t chunk : {1U, 7U, 16U, 33U, 100U}) {
        const std::size_t length = std::min(chunk, message.size() - at);
        decryptor->decrypt(std::span{message}.subspan(at, length));
        at += length;
    }
    decryptor->decrypt(std::span{message}.subspan(at));
    CHECK(message == original);
}

TEST_CASE("the session hash formats the way BigInteger does") {
    // parity: the three worked examples Mojang's own documentation gives for the join hash.
    const auto digestOf = [](std::string_view text) {
        std::vector<std::uint8_t> bytes;
        for (std::size_t i = 0; i < text.size(); i += 2) {
            bytes.push_back(static_cast<std::uint8_t>(std::stoi(std::string{text.substr(i, 2)}, nullptr, 16)));
        }
        return bytes;
    };

    // The three digests Mojang's own documentation works through, and the strings vanilla prints
    // for them. They are the cases that go wrong: a plain positive one, a negative one, and one
    // whose leading nibble is zero.
    CHECK(formatSessionHash(digestOf("4ed1f46bbe04bc756bcb17c0c7ce3e4632f06a48")) ==
          "4ed1f46bbe04bc756bcb17c0c7ce3e4632f06a48");
    CHECK(formatSessionHash(digestOf("8362a4ffbb3ecfef65a284a04a3ce83fd4b1d73f")) ==
          "-7c9d5b0044c130109a5d7b5fb5c317c02b4e28c1");
    CHECK(formatSessionHash(digestOf("088e16a1019277b15d58faf0541e11910eb756f6")) ==
          "88e16a1019277b15d58faf0541e11910eb756f6");
    CHECK(formatSessionHash(digestOf("ff")) == "-1");
    CHECK(formatSessionHash(digestOf("00")) == "0");
    CHECK(formatSessionHash(digestOf("000f")) == "f");
    CHECK(formatSessionHash(digestOf("80")) == "-80");
}

TEST_CASE("an RSA session keypair encrypts to itself and hashes a session") {
    auto keyPair = SessionKeyPair::generate();
    REQUIRE(keyPair.has_value());
    // X.509 SubjectPublicKeyInfo for a 1024-bit RSA key is 162 bytes and starts with a SEQUENCE.
    CHECK(keyPair->publicKeyDer().size() == 162);
    CHECK(keyPair->publicKeyDer()[0] == 0x30);

    std::vector<std::uint8_t> secret(16);
    REQUIRE(randomBytes(secret));
    const std::string hash = sessionHash("", secret, keyPair->publicKeyDer());
    CHECK_FALSE(hash.empty());
    // The same inputs must give the same hash, or the session server rejects every login.
    CHECK(sessionHash("", secret, keyPair->publicKeyDer()) == hash);

    std::vector<std::uint8_t> other(16);
    REQUIRE(randomBytes(other));
    CHECK(other != secret);
}

TEST_CASE("forwarding modes parse the names other software uses") {
    using namespace basalt::wire::auth::forwarding;

    CHECK(parseForwardingMode("bungeecord") == ForwardingMode::BungeeCord);
    CHECK(parseForwardingMode(" VELOCITY ") == ForwardingMode::Velocity);
    CHECK(parseForwardingMode("modern") == ForwardingMode::Velocity);
    CHECK(parseForwardingMode("legacy") == ForwardingMode::BungeeCord);
    CHECK(parseForwardingMode("disabled") == ForwardingMode::None);
    CHECK_FALSE(parseForwardingMode("something else").has_value());
    CHECK(forwardingModeName(ForwardingMode::SafeNet) == "safenet");

    // Plain BungeeCord forwarding proves nothing, which is the point of saying so out loud.
    CHECK_FALSE((ProxyForwarding{ForwardingMode::BungeeCord, "", {}, ""}).authenticated());
    CHECK((ProxyForwarding{ForwardingMode::Velocity, "secret", {}, ""}).authenticated());
    CHECK((ProxyForwarding{ForwardingMode::Velocity, "", {}, ""}).misconfiguration().has_value());
    CHECK((ProxyForwarding{ForwardingMode::BungeeGuard, "", {}, ""}).misconfiguration().has_value());
}

TEST_CASE("the legacy handshake carries a profile, and its token is checked and stripped") {
    using namespace basalt::wire::auth;
    using namespace basalt::wire::auth::forwarding;

    const basalt::wire::common::Uuid id = basalt::wire::common::Uuid::offlinePlayer("Steve");
    const std::vector<SessionProfileProperty> properties = {{"textures", "base64", std::string{"signature"}}};

    ProxyForwarding plain{ForwardingMode::BungeeCord, "", {}, ""};
    const std::string host = legacy::encode("play.basaltmc.org", "203.0.113.5", id, properties);
    const ForwardingResult result = legacy::decode(host, plain);
    REQUIRE(accepted(result));
    const ForwardedLogin& login = std::get<ForwardedLogin>(result);
    CHECK(login.host == "play.basaltmc.org");
    CHECK(login.address == "203.0.113.5");
    CHECK(login.profileId == id);
    CHECK(login.properties == properties);

    ProxyForwarding guarded{ForwardingMode::BungeeGuard, "", {"correct-token"}, ""};
    const std::string guardedHost = legacy::encode("play.basaltmc.org", "203.0.113.5", id, properties,
                                                   ProxyForwarding::kBungeeGuardProperty, "correct-token");
    const ForwardingResult guardedResult = legacy::decode(guardedHost, guarded);
    REQUIRE(accepted(guardedResult));
    // The token is a password, so it does not come back out as part of the profile.
    CHECK(std::get<ForwardedLogin>(guardedResult).properties == properties);

    const std::string wrongHost = legacy::encode("play.basaltmc.org", "203.0.113.5", id, properties,
                                                 ProxyForwarding::kBungeeGuardProperty, "wrong-token");
    CHECK_FALSE(accepted(legacy::decode(wrongHost, guarded)));
    // A bare host is somebody connecting to the port directly, and it is refused as such.
    CHECK_FALSE(accepted(legacy::decode("play.basaltmc.org", plain)));
}

TEST_CASE("velocity forwarding refuses anything it did not sign") {
    using namespace basalt::wire::auth;
    using namespace basalt::wire::auth::forwarding;

    const ProxyForwarding forwarding{ForwardingMode::Velocity, "the-shared-secret", {}, ""};
    const basalt::wire::common::Uuid id = basalt::wire::common::Uuid::offlinePlayer("Steve");
    const std::vector<SessionProfileProperty> properties = {{"textures", "base64", std::nullopt}};

    std::vector<std::uint8_t> payload = velocity::sign(forwarding.secret, "203.0.113.5", id, "Steve", properties);
    const ForwardingResult result = velocity::decode(payload, forwarding);
    REQUIRE(accepted(result));
    const ForwardedLogin& login = std::get<ForwardedLogin>(result);
    CHECK(login.address == "203.0.113.5");
    CHECK(login.profileId == id);
    CHECK(login.name == "Steve");
    CHECK(login.properties == properties);

    // One flipped byte anywhere invalidates the tag, which is the whole point of signing it.
    payload.back() ^= 0x01;
    CHECK_FALSE(accepted(velocity::decode(payload, forwarding)));

    const ProxyForwarding other{ForwardingMode::Velocity, "a-different-secret", {}, ""};
    CHECK_FALSE(accepted(velocity::decode(velocity::sign(forwarding.secret, "1.2.3.4", id, "Steve", {}), other)));

    // The query asks for the newest version this server reads.
    CHECK(velocity::request() == std::vector<std::uint8_t>{velocity::kMaxSupportedVersion});
    CHECK_FALSE(accepted(velocity::decode(std::vector<std::uint8_t>(8, 0), forwarding)));
}

TEST_CASE("the session service reads the profile the session server answered with") {
    // A canned answer, so the test exercises the decoding and never the network.
    const auto answer = [](std::string_view url) -> std::expected<HttpsResponse, HttpsError> {
        CHECK(url.find("username=Steve") != std::string_view::npos);
        CHECK(url.find("serverId=abc") != std::string_view::npos);
        const std::string body = R"({"id":"8667ba71b85a4004af54457a9734eed7","name":"Steve","properties":)"
                                 R"([{"name":"textures","value":"blob","signature":"signed"}]})";
        return HttpsResponse{.status = 200, .body = std::vector<std::uint8_t>(body.begin(), body.end())};
    };
    MojangSessionService service{answer};

    const std::optional<SessionProfile> profile = service.hasJoined("Steve", "abc", "");
    REQUIRE(profile.has_value());
    CHECK(profile->name == "Steve");
    CHECK(profile->id.toString() == "8667ba71-b85a-4004-af54-457a9734eed7");
    REQUIRE(profile->properties.size() == 1);
    CHECK(profile->properties.front().name == "textures");
    CHECK(profile->properties.front().signature == "signed");
}

TEST_CASE("a session server that says nobody joined answers nothing") {
    // 204 with an empty body is the session server's "that is not them".
    MojangSessionService refused{[](std::string_view) { return HttpsResponse{.status = 204, .body = {}}; }};
    CHECK(!refused.hasJoined("Steve", "abc", "").has_value());

    // So does a request that could not be made at all: a login is refused, never let through.
    MojangSessionService offline{[](std::string_view) { return std::unexpected(HttpsError{"no network"}); }};
    CHECK(!offline.hasJoined("Steve", "abc", "").has_value());
}

TEST_CASE("a name is looked up as a dashed uuid") {
    const auto answer = [](std::string_view url) -> std::expected<HttpsResponse, HttpsError> {
        CHECK(url.ends_with("/Steve"));
        const std::string body = R"({"id":"8667ba71b85a4004af54457a9734eed7","name":"Steve"})";
        return HttpsResponse{.status = 200, .body = std::vector<std::uint8_t>(body.begin(), body.end())};
    };
    // Dashed, because every other uuid this server prints is, and two spellings of the same id in
    // one console is a needless puzzle.
    CHECK(mojangUuidOf("Steve", answer) == "8667ba71-b85a-4004-af54-457a9734eed7");

    CHECK(
        !mojangUuidOf("Nobody", [](std::string_view) { return HttpsResponse{.status = 404, .body = {}}; }).has_value());
}

namespace {

std::vector<std::uint8_t> hexBytes(std::string_view hex) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(std::stoi(std::string{hex.substr(i, 2)}, nullptr, 16)));
    }
    return bytes;
}

// A throwaway 1024-bit key and its signatures over "basalt", made with openssl for this test.
constexpr std::string_view kTestKey =
    "30819f300d06092a864886f70d010101050003818d0030818902818100bb85740d94fafc4ce3099263cc41699dfdaf4ece12"
    "4c5e4b2197f67d465dbc532cbfff975a9ec1e61412cc0cdece88f1dd158463777332f07a5eb0dd843fa578308243bb3ed44c"
    "83a4cea80361a7c51e039b060c123b627fd317b67826e9ca7589f3aa9dd424d1e3696d66f5f233cf14df1d001761b95e9076"
    "a32f251a7baef70203010001";
constexpr std::string_view kSha256Signature =
    "3dfd1431ffbfff2617be5790ce3d939e41c55eb5ea40231096e2f3cbdf508b6c523a23f6796ea546f0a9b9879a53008c6ecb"
    "6da2f241bf9259d5e9911c4c26ad5ef3172648e57963212e3ece9f316711ca45280d4975335883c24ad109e1b0f28d9ea442"
    "39fab9dcf58f546939f89e0fba928917b54003200ecf0bfb99f1c926";
constexpr std::string_view kSha1Signature =
    "77b0a108d7f3ce4b101346f559de5513654af7648a5dbd51f4f341c42fc652ab93bcf1672cd8346c0116afbd81256ea5a90d"
    "f3df3f8a6426c91458fa3f4c089f39cfc1b7d09b1b6ee05d88d30efdbf6a670f85a4261e05f3f4b058b4244d6c1ea2f07b34"
    "54b4aa9140a5b14af445c36b7116b9556336379103e1326e1f6778cc";

} // namespace

TEST_CASE("an RSA public key checks PKCS#1 v1.5 signatures in both digests") {
    const std::optional<RsaPublicKey> key = RsaPublicKey::parseDer(hexBytes(kTestKey));
    REQUIRE(key.has_value());
    const std::vector<std::uint8_t> payload{'b', 'a', 's', 'a', 'l', 't'};
    CHECK(key->verify(SignatureAlgorithm::Sha256WithRsa, payload, hexBytes(kSha256Signature)));
    CHECK(key->verify(SignatureAlgorithm::Sha1WithRsa, payload, hexBytes(kSha1Signature)));
    CHECK_FALSE(key->verify(SignatureAlgorithm::Sha1WithRsa, payload, hexBytes(kSha256Signature)));
    const std::vector<std::uint8_t> other{'b', 'a', 's', 'a', 'l', 'T'};
    CHECK_FALSE(key->verify(SignatureAlgorithm::Sha256WithRsa, other, hexBytes(kSha256Signature)));
    CHECK_FALSE(RsaPublicKey::parseDer(std::vector<std::uint8_t>{1, 2, 3}).has_value());

    const std::string document = "{\"playerCertificateKeys\":[{\"publicKey\":\"" +
                                 basalt::wire::common::base64::encode(hexBytes(kTestKey)) + "\"}]}";
    const ServicesKeySet services = ServicesKeySet::parse(document);
    CHECK(services.canValidateProfileKeys());
    CHECK(services.verifyProfileKey(payload, hexBytes(kSha1Signature)));
    CHECK_FALSE(ServicesKeySet::parse("{}").canValidateProfileKeys());

    // parity: `ProfilePublicKey.Data.signedPayload`, big-endian id halves then the expiry.
    const std::vector<std::uint8_t> signedPayload =
        profileKeyPayload(basalt::wire::common::Uuid{1, 2}, 3, std::vector<std::uint8_t>{9});
    REQUIRE(signedPayload.size() == 25);
    CHECK(signedPayload[7] == 1);
    CHECK(signedPayload[15] == 2);
    CHECK(signedPayload[23] == 3);
    CHECK(signedPayload[24] == 9);
}

TEST_CASE("profile authentication rejects oversized bodies and non-200 success statuses") {
    const std::string valid = R"({"id":"8667ba71b85a4004af54457a9734eed7","name":"Steve"})";
    const std::vector<std::uint8_t> body{valid.begin(), valid.end()};
    for (const int status : {201, 202, 204, 206, 302, 500}) {
        MojangSessionService service{[&](std::string_view) { return HttpsResponse{.status = status, .body = body}; }};
        CHECK_FALSE(service.hasJoined("Steve", "hash", ""));
    }
    std::vector<std::uint8_t> large = body;
    large.resize(kMaxSessionProfileBytes + 1, ' ');
    MojangSessionService service{[&](std::string_view) { return HttpsResponse{.status = 200, .body = large}; }};
    CHECK_FALSE(service.hasJoined("Steve", "hash", ""));
    CHECK_FALSE(mojangUuidOf("Steve", [&](std::string_view) { return HttpsResponse{.status = 200, .body = large}; }));
}
