#include <basalt/wire/auth/forwarding/BasaltForwarding.hpp>

#include <doctest.h>

#include <array>

using namespace basalt::wire::auth::forwarding::native;

namespace {

constexpr std::array<std::uint8_t, 32> kSeed = {0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60, 0xba, 0x84, 0x4a,
                                                0xf4, 0x92, 0xec, 0x2c, 0xc4, 0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32,
                                                0x69, 0x19, 0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60};

Identity sampleIdentity() {
    Identity identity;
    identity.profileId = basalt::wire::common::Uuid{1, 2};
    identity.name = "Steve";
    identity.properties.push_back({"textures", "value", std::string{"sig"}});
    identity.address = "203.0.113.9";
    identity.port = 51234;
    identity.virtualHost = "play.example.org";
    identity.virtualPort = 25565;
    identity.onlineMode = true;
    identity.proxyId = "edge-1";
    return identity;
}

} // namespace

TEST_CASE("native forwarding signs per connection and backend, and rejects altered fields") {
    const SigningKey key = SigningKey::fromSeed(kSeed);
    // RFC 8032 section 7.1, test 1
    CHECK(key.publicKey()[0] == 0xd7);
    CHECK(key.publicKey()[31] == 0x1a);

    const std::array<PublicKey, 1> trusted = {key.publicKey()};
    const Request request = *newRequest("lobby");
    CHECK(decodeRequest(encodeRequest(request)) == request);

    const std::vector<std::uint8_t> response = sign(key, request, kVersion1, sampleIdentity());
    REQUIRE_FALSE(response.empty());
    const VerifyResult ok = verify(response, request, trusted);
    REQUIRE(std::holds_alternative<Verified>(ok));
    CHECK(std::get<Verified>(ok).identity == sampleIdentity());
    CHECK(std::get<Verified>(ok).keyId == keyId(key.publicKey()));

    Request otherChallenge = request;
    otherChallenge.challenge[0] ^= 1;
    CHECK(std::holds_alternative<std::string>(verify(response, otherChallenge, trusted)));

    Request otherBackend = request;
    otherBackend.backendId = "survival";
    CHECK(std::holds_alternative<std::string>(verify(response, otherBackend, trusted)));

    std::vector<std::uint8_t> altered = response;
    altered.back() ^= 1;
    CHECK(std::holds_alternative<std::string>(verify(altered, request, trusted)));

    const SigningKey stranger = *SigningKey::generate();
    const std::array<PublicKey, 1> others = {stranger.publicKey()};
    CHECK(std::holds_alternative<std::string>(verify(response, request, others)));

    CHECK(sign(key, request, 2, sampleIdentity()).empty());
}
