#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/lua/LuaSandbox.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

using namespace basalt::wire::lua;

namespace {
[[nodiscard]] std::optional<LuaFailure> run(LuaSandbox& sandbox, std::string_view source) {
    const std::expected<LuaChunk, LuaFailure> chunk = sandbox.compile(source, "test.lua");
    if (!chunk.has_value()) {
        return chunk.error();
    }
    const std::expected<void, LuaFailure> result = sandbox.run(*chunk);
    if (!result.has_value()) {
        return result.error();
    }
    return std::nullopt;
}

[[nodiscard]] bool isNil(LuaSandbox& sandbox, std::string_view expression) {
    std::vector<std::string> printed;
    const std::optional<LuaFailure> failure =
        run(sandbox, std::string{"print(("} + std::string{expression} + ") == nil)");
    return !failure.has_value();
}
} // namespace

TEST_CASE("a plugin cannot reach what the sandbox took away") {
    std::vector<std::string> printed;
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", {}, 0, {}, [&](std::string_view line) { printed.emplace_back(line); });
    REQUIRE(sandbox != nullptr);

    CHECK_FALSE(run(*sandbox, R"(
        local gone = {"io", "debug", "dofile", "loadfile", "collectgarbage", "package"}
        for _, name in ipairs(gone) do
            assert(_G[name] == nil, name .. " is still reachable")
        end
        local osGone = {"execute", "exit", "getenv", "remove", "rename", "tmpname", "setlocale"}
        for _, name in ipairs(osGone) do
            assert(os[name] == nil, "os." .. name .. " is still reachable")
        end
    )")
                    .has_value());

    CHECK_FALSE(run(*sandbox, R"(
        assert(type(os.time) == "function")
        assert(type(os.date) == "function")
        assert(type(string.format) == "function")
        assert(type(table.insert) == "function")
        assert(type(math.floor) == "function")
        assert(type(pcall) == "function")
        assert(type(coroutine.create) == "function")
    )")
                    .has_value());
    CHECK(isNil(*sandbox, "io"));
}

TEST_CASE("a runaway plugin is stopped by its instruction budget") {
    const std::unique_ptr<LuaSandbox> sandbox = LuaSandbox::create("arena", LuaLimits{.instructionsPerTurn = 20000});
    REQUIRE(sandbox != nullptr);

    const std::optional<LuaFailure> failure = run(*sandbox, "while true do end");
    REQUIRE(failure.has_value());
    CHECK(failure->outOfBudget());
    CHECK_FALSE(failure->outOfMemory());
    CHECK(failure->plugin == "arena");
    CHECK(failure->report().starts_with("[arena] "));
    CHECK(sandbox->instructionsLeft() > -2000);

    sandbox->refill();
    CHECK(sandbox->instructionsLeft() == 20000);
    CHECK_FALSE(
        run(*sandbox, "local total = 0 for i = 1, 10 do total = total + i end assert(total == 55)").has_value());
}

TEST_CASE("a plugin cannot take more memory than it is allowed") {
    const std::unique_ptr<LuaSandbox> sandbox =
        LuaSandbox::create("arena", LuaLimits{.instructionsPerTurn = 100000000, .memoryBytes = 512 * 1024});
    REQUIRE(sandbox != nullptr);

    const std::optional<LuaFailure> failure =
        run(*sandbox, "local t = {} for i = 1, 10000000 do t[i] = string.rep('x', 64) end");
    REQUIRE(failure.has_value());
    CHECK(failure->outOfMemory());
    CHECK(sandbox->memoryUsed() <= 512 * 1024);
}
