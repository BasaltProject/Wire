#pragma once

#include <basalt/wire/lua/LuaFailure.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct lua_State;

namespace basalt::wire::lua {
struct LuaLimits {
    std::int64_t instructionsPerTurn = 250000;
    std::size_t memoryBytes = 32ULL * 1024 * 1024;
};

struct LuaChunk {
    std::string chunkName;
    std::vector<std::uint8_t> bytecode;
};

class LuaSandbox {
public:
    using SourceFinder = std::function<std::optional<std::vector<std::uint8_t>>(std::string_view name)>;
    using PrintSink = std::function<void(std::string_view line)>;

    LuaSandbox(const LuaSandbox&) = delete;
    LuaSandbox& operator=(const LuaSandbox&) = delete;
    LuaSandbox(LuaSandbox&&) = delete;
    LuaSandbox& operator=(LuaSandbox&&) = delete;
    ~LuaSandbox();

    [[nodiscard]] static std::unique_ptr<LuaSandbox> create(std::string name, LuaLimits limits = {},
                                                            std::uint64_t seed = 0, SourceFinder sources = {},
                                                            PrintSink printLine = {});

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    [[nodiscard]] const LuaLimits& limits() const noexcept { return limits_; }

    [[nodiscard]] lua_State* state() const noexcept { return state_; }

    [[nodiscard]] std::expected<LuaChunk, LuaFailure> compile(std::string_view source, std::string chunkName);

    [[nodiscard]] std::expected<void, LuaFailure> bind(const LuaChunk& chunk);

    [[nodiscard]] std::expected<void, LuaFailure> run(const LuaChunk& chunk);

    void refill();

    /// Stops whatever is running, from a thread that is not the one running it.
    void interrupt();

    void resetMemoryCount();

    [[nodiscard]] std::size_t memoryUsed() const noexcept { return memoryUsed_.load(std::memory_order_relaxed); }

    [[nodiscard]] std::int64_t instructionsLeft() const noexcept { return budget_.load(std::memory_order_relaxed); }

    [[nodiscard]] LuaFailure failureFromStack();

    /// Lets `require` load precompiled chunks; only for code the host has already verified.
    void allowBytecode(bool allowed) noexcept { bytecodeAllowed_ = allowed; }

private:
    LuaSandbox(std::string name, LuaLimits limits, SourceFinder sources, PrintSink printLine);

    friend struct SandboxAccess;

    void install(std::uint64_t seed);
    void strip();

    std::string name_;
    LuaLimits limits_;
    SourceFinder sources_;
    PrintSink printLine_;
    bool bytecodeAllowed_ = false;
    lua_State* state_ = nullptr;

    std::atomic<std::int64_t> budget_{0};
    std::atomic<bool> interrupted_{false};
    std::atomic<std::size_t> memoryUsed_{0};
    std::size_t memoryCeiling_ = 0;
};
} // namespace basalt::wire::lua
