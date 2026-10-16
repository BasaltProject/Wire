#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/lua/LuaCall.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::wire::lua;

namespace {
[[nodiscard]] bool push(LuaSandbox& sandbox, std::string_view source) {
    const std::expected<LuaChunk, LuaFailure> chunk = sandbox.compile(source, "test.lua");
    return chunk.has_value() && sandbox.bind(*chunk).has_value();
}
} // namespace

TEST_CASE("a plugin that waits gives the turn back and is picked up later") {
    std::vector<std::string> printed;
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", {}, 0, {}, [&](std::string_view line) { printed.emplace_back(line); });
    REQUIRE(sandbox != nullptr);

    std::vector<LuaCall::Resume> pending;
    installWaitingFunction(*sandbox, "wait", [&](LuaCall::Resume resume) { pending.push_back(std::move(resume)); });

    REQUIRE(push(*sandbox, R"(
        print("before")
        wait(20)
        print("after")
        wait(20)
        print("done")
    )"));

    std::int32_t done = 0;
    const std::shared_ptr<LuaCall> call = LuaCall::start(*sandbox, 0, [&](std::optional<LuaFailure>) { ++done; });

    CHECK(done == 0);
    CHECK_FALSE(call->finished());
    CHECK(call->paused());
    REQUIRE(printed.size() == 1);
    CHECK(printed[0] == "before");
    REQUIRE(pending.size() == 1);

    pending.front()();
    CHECK(done == 0);
    REQUIRE(printed.size() == 2);
    CHECK(printed[1] == "after");
    REQUIRE(pending.size() == 2);

    pending[1]();
    CHECK(done == 1);
    CHECK(call->finished());
    REQUIRE(printed.size() == 3);
    CHECK(printed[2] == "done");
}

TEST_CASE("waiting where the interpreter cannot pause is refused, not blocked") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena");
    REQUIRE(sandbox != nullptr);
    std::int32_t arranged = 0;
    installWaitingFunction(*sandbox, "wait", [&](LuaCall::Resume) { ++arranged; });

    REQUIRE(push(*sandbox, R"(
        local values = {3, 1, 2}
        table.sort(values, function(a, b) wait(1) return a < b end)
    )"));

    std::optional<LuaFailure> reported;
    static_cast<void>(
        LuaCall::start(*sandbox, 0, [&](std::optional<LuaFailure> failure) { reported = std::move(failure); }));
    REQUIRE(reported.has_value());
    CHECK(reported->message.find("'wait' cannot be used from here") != std::string::npos);
    CHECK(arranged == 0);
}
