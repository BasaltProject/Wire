#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/auth/Cfb8Cipher.hpp>
#include <basalt/wire/auth/MojangSessionService.hpp>
#include <basalt/wire/auth/SessionKeyPair.hpp>
#include <basalt/wire/auth/SignatureValidator.hpp>
#include <basalt/wire/auth/forwarding/LegacyForwarding.hpp>
#include <basalt/wire/auth/forwarding/VelocityForwarding.hpp>
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::wire::auth;

TEST_CASE("AES-CFB8 matches the NIST vector and survives odd chunking") {
    const std::vector<std::uint8_t> key = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                                           0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    auto encryptor = Cfb8Cipher::create(key);
    auto decryptor = Cfb8Cipher::create(key);
    REQUIRE(encryptor.has_value());
    REQUIRE(decryptor.has_value());

    std::vector<std::uint8_t> message(300);
    for (std::size_t i = 0; i < message.size(); ++i) {
        message[i] = static_cast<std::uint8_t>(i * 11);
    }
    const std::vector<std::uint8_t> original = message;

    // Odd-sized chunks: the keystream is continuous, so a chunk boundary must not disturb it.
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
    CHECK(std::get<ForwardedLogin>(guardedResult).properties == properties);

    const std::string wrongHost = legacy::encode("play.basaltmc.org", "203.0.113.5", id, properties,
                                                 ProxyForwarding::kBungeeGuardProperty, "wrong-token");
    CHECK_FALSE(accepted(legacy::decode(wrongHost, guarded)));
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

    payload.back() ^= 0x01;
    CHECK_FALSE(accepted(velocity::decode(payload, forwarding)));

    const ProxyForwarding other{ForwardingMode::Velocity, "a-different-secret", {}, ""};
    CHECK_FALSE(accepted(velocity::decode(velocity::sign(forwarding.secret, "1.2.3.4", id, "Steve", {}), other)));

    CHECK(velocity::request() == std::vector<std::uint8_t>{velocity::kMaxSupportedVersion});
    CHECK_FALSE(accepted(velocity::decode(std::vector<std::uint8_t>(8, 0), forwarding)));
}

namespace {

std::vector<std::uint8_t> hexBytes(std::string_view hex) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(std::stoi(std::string{hex.substr(i, 2)}, nullptr, 16)));
    }
    return bytes;
}

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
