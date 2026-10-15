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

TEST_CASE("bad RSA shape and nonce are rejected without spending an HTTP request") {
    Fixture fixture;
    int calls = 0;
    const AuthenticationFetch fetch = [&](auto, auto, auto) {
        ++calls;
        return std::expected<HttpsResponse, HttpsError>{profile("Tester")};
    };
    fixture.request.encryptedSecret.pop_back();
    const auto malformed = authenticateLogin(fixture.key, fixture.request, {}, fetch);
    REQUIRE_FALSE(malformed);
    CHECK(malformed.error().kind == LoginAuthenticationError::InvalidRequest);
    fixture.request.encryptedSecret = test::encryptRsa(fixture.key.publicKeyDer(), fixture.secret);
    fixture.request.challenge[0] ^= 1;
    const auto forged = authenticateLogin(fixture.key, fixture.request, {}, fetch);
    REQUIRE_FALSE(forged);
    CHECK(forged.error().kind == LoginAuthenticationError::InvalidChallenge);
    CHECK_FALSE(forged.error().secret);
    CHECK(calls == 0);
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

TEST_CASE("deadline and cancellation cover the injected session lookup") {
    Fixture fixture;
    std::stop_source canceled;
    canceled.request_stop();
    const auto stopped = authenticateLogin(fixture.key, fixture.request, canceled.get_token());
    REQUIRE_FALSE(stopped);
    CHECK(stopped.error().kind == LoginAuthenticationError::Canceled);
    fixture.request.deadline = std::chrono::steady_clock::now() + 10ms;
    const auto late = authenticateLogin(fixture.key, fixture.request, {}, [](auto, auto, auto) {
        std::this_thread::sleep_for(20ms);
        return std::expected<HttpsResponse, HttpsError>{profile("Tester")};
    });
    REQUIRE_FALSE(late);
    CHECK(late.error().kind == LoginAuthenticationError::TimedOut);
}

TEST_CASE("Mojang query and path values are escaped instead of changing request structure") {
    MojangSessionService service{[](auto url) {
        CHECK(url.find("username=A%26B&serverId=-12&ip=2001%3Adb8%3A%3A1") != std::string::npos);
        return std::expected<HttpsResponse, HttpsError>{HttpsResponse{204, {}}};
    }};
    CHECK_FALSE(service.hasJoined("A&B", "-12", "2001:db8::1"));
    CHECK_FALSE(mojangUuidOf("A/B?x", [](auto url) {
        CHECK(url.ends_with("/A%2FB%3Fx"));
        return std::expected<HttpsResponse, HttpsError>{HttpsResponse{404, {}}};
    }));
}

TEST_CASE("shared key decryption and nonce generation are safe across concurrent workers") {
    Fixture fixture;
    std::vector<std::future<std::expected<AuthenticatedLogin, LoginAuthenticationFailure>>> workers;
    for (int i = 0; i < 4; ++i) {
        workers.push_back(std::async(std::launch::async, [&] {
            std::array<std::uint8_t, 4> nonce{};
            if (!randomBytes(nonce))
                throw std::runtime_error("fixture nonce generation failed");
            return authenticateLogin(fixture.key, fixture.request, {}, [](auto, auto, auto) {
                return std::expected<HttpsResponse, HttpsError>{profile("Tester")};
            });
        }));
    }
    for (auto& worker : workers) {
        const auto result = worker.get();
        REQUIRE(result);
        CHECK(result->secret == fixture.secret);
    }
}
