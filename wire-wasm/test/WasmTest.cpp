#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "GuestBytes.hpp"

#include <basalt/wire/wasm/Instance.hpp>

#include <doctest.h>

#include <chrono>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace basalt::wire::wasm::test {
namespace {
struct Host {
    std::vector<std::string> logs;
    std::vector<std::int32_t> levels;
    std::int32_t eventToken = 0;
};

void hostLog(void* user, Call& call) {
    auto* host = static_cast<Host*>(user);
    const auto text = call.memory().string(call.i32(1), call.i32(2));
    REQUIRE(text.has_value());
    host->levels.push_back(call.i32(0));
    host->logs.emplace_back(*text);
}

void hostEventOn(void* user, Call& call) {
    static_cast<Host*>(user)->eventToken = call.i32(3);
    call.ret(kOk);
}

void hostConfigI64(void*, Call& call) { call.ret(std::int64_t{40}); }

void hostConfigStr(void*, Call& call) { call.ret(call.memory().put(call.i32(2), call.i32(3), "forty-two")); }

std::span<const std::uint8_t> bytesOf(const unsigned char* data, std::size_t size) {
    return {static_cast<const std::uint8_t*>(data), size};
}

Result<Instance> instantiate(const Module& module, Host& host) {
    const HostImport imports[] = {
        {"log", hostLog, &host},
        {"event_on", hostEventOn, &host},
        {"config_i64", hostConfigI64, nullptr},
        {"config_str", hostConfigStr, nullptr},
    };
    return Instance::create(module, InstanceOptions{imports, {}});
}
} // namespace

TEST_CASE("a guest runs through the host imports, traps, is interrupted and stays usable") {
    auto engine = Engine::create();
    REQUIRE(engine.has_value());
    auto module = Module::compile(*engine, bytesOf(kContractGuest, sizeof kContractGuest));
    REQUIRE_MESSAGE(module.has_value(), module.error().message);

    Host host;
    auto instance = instantiate(*module, host);
    REQUIRE_MESSAGE(instance.has_value(), instance.error().message);
    REQUIRE(instance->init().has_value());
    CHECK(host.logs == std::vector<std::string>{"hello"});
    CHECK(host.eventToken == 1);

    REQUIRE(instance->callback(1, 2).has_value());
    CHECK(host.levels.back() == 42);
    CHECK(host.logs.back() == "forty-two");

    const auto trapped = instance->callback(3, 0);
    REQUIRE_FALSE(trapped.has_value());
    CHECK(trapped.error().code == ErrorCode::Trap);

    std::jthread watchdog([&instance] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        instance->interrupt();
    });
    const auto interrupted = instance->callback(2, 0);
    REQUIRE_FALSE(interrupted.has_value());
    CHECK(interrupted.error().code == ErrorCode::Interrupted);
    watchdog.join();

    CHECK(instance->callback(1, 2).has_value());

    Host other;
    auto second = instantiate(*module, other);
    REQUIRE(second.has_value());
    CHECK(second->init().has_value());
    CHECK(other.logs.size() == 1);

    if (engine->supportsSerialization()) {
        auto compiled = module->serialize();
        REQUIRE(compiled.has_value());
        auto restored = Module::deserialize(*engine, *compiled);
        REQUIRE_MESSAGE(restored.has_value(), restored.error().message);
        Host third;
        auto instanceAgain = instantiate(*restored, third);
        REQUIRE(instanceAgain.has_value());
        CHECK(instanceAgain->init().has_value());
    }
}

TEST_CASE("a module importing outside the ABI is rejected") {
    auto engine = Engine::create();
    REQUIRE(engine.has_value());
    const auto module = Module::compile(*engine, bytesOf(kBadImportGuest, sizeof kBadImportGuest));
    REQUIRE_FALSE(module.has_value());
    CHECK(module.error().code == ErrorCode::InvalidModule);
}

TEST_CASE("the proxy profile shares the generic imports and keeps the server-only ones apart") {
    const auto allowed = [](Profile profile, std::string_view module, std::string_view name) {
        return inProfile(profile, abiFunctions()[*findAbiFunction(module, name)]);
    };
    CHECK(allowed(Profile::Server, "basalt", "log"));
    CHECK(allowed(Profile::Proxy, "basalt", "log"));
    CHECK(allowed(Profile::Server, "basalt", "menu_open"));
    CHECK_FALSE(allowed(Profile::Proxy, "basalt", "menu_open"));
    CHECK(allowed(Profile::Proxy, "basalt_proxy", "player_connect"));
    CHECK_FALSE(allowed(Profile::Server, "basalt_proxy", "player_connect"));
}

} // namespace basalt::wire::wasm::test
