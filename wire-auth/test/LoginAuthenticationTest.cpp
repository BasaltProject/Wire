#include "support/RsaClient.hpp"

#include <basalt/wire/auth/LoginAuthentication.hpp>
#include <basalt/wire/auth/MojangSessionService.hpp>

#include <doctest.h>

#include <atomic>
#include <future>
#include <thread>

using namespace basalt::wire::auth;
using namespace std::chrono_literals;

namespace {
struct Fixture {
    SessionKeyPair key;
    LoginSecret secret{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    OnlineLoginRequest request;

    static SessionKeyPair generate() {
        auto result = SessionKeyPair::generate();
        if (!result)
            throw std::runtime_error("fixture key generation failed");
        return std::move(*result);
    }

    Fixture() : key(generate()) {
        request.name = "Tester";
        request.challenge = {1, 2, 3, 4};
        request.deadline = std::chrono::steady_clock::now() + 2s;
        request.encryptedSecret = test::encryptRsa(key.publicKeyDer(), secret);
        request.encryptedChallenge = test::encryptRsa(key.publicKeyDer(), request.challenge);
    }
};

HttpsResponse profile(std::string_view name) {
    const auto id = basalt::wire::common::Uuid::offlinePlayer(name).toUndashedString();
    const std::string text = "{\"id\":\"" + id + "\",\"name\":\"" + std::string{name} + "\"}";
    return {200, {text.begin(), text.end()}};
}
} // namespace

TEST_CASE("online authentication binds encrypted nonce, secret, digest and returned profile") {
    Fixture fixture;
    const auto result = authenticateLogin(fixture.key, fixture.request, {}, [&](auto url, auto deadline, auto stop) {
        CHECK(url.find("username=Tester&serverId=" + sessionHash("", fixture.secret, fixture.key.publicKeyDer())) !=
              std::string::npos);
        CHECK(deadline == fixture.request.deadline);
        CHECK_FALSE(stop.stop_requested());
        return std::expected<HttpsResponse, HttpsError>{profile("Tester")};
    });
    REQUIRE(result);
    CHECK(result->secret == fixture.secret);
    CHECK(result->profile.name == "Tester");
}

TEST_CASE("HTTP failures retain valid cipher material without inventing an offline identity") {
    Fixture fixture;
    const auto rejected = authenticateLogin(fixture.key, fixture.request, {}, [](auto, auto, auto) {
        return std::expected<HttpsResponse, HttpsError>{HttpsResponse{204, {}}};
    });
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().kind == LoginAuthenticationError::Rejected);
    REQUIRE(rejected.error().secret);
    CHECK(*rejected.error().secret == fixture.secret);
    const auto wrongIdentity = authenticateLogin(fixture.key, fixture.request, {}, [](auto, auto, auto) {
        return std::expected<HttpsResponse, HttpsError>{profile("Another")};
    });
    REQUIRE_FALSE(wrongIdentity);
    CHECK(wrongIdentity.error().kind == LoginAuthenticationError::Rejected);
    const auto unavailable = authenticateLogin(fixture.key, fixture.request, {}, [](auto, auto, auto) {
        return std::expected<HttpsResponse, HttpsError>{HttpsResponse{503, {}}};
    });
    REQUIRE_FALSE(unavailable);
    CHECK(unavailable.error().kind == LoginAuthenticationError::Unavailable);
    REQUIRE(unavailable.error().secret);
}
