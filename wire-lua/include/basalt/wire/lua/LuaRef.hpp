#pragma once

#include <cstdint>
#include <utility>

struct lua_State;

namespace basalt::wire::lua {
class LuaArgs;
class LuaSandbox;

class LuaRef {
public:
    LuaRef() = default;

    LuaRef(const LuaRef&) = delete;
    LuaRef& operator=(const LuaRef&) = delete;

    LuaRef(LuaRef&& other) noexcept
        : state_(std::exchange(other.state_, nullptr)), ref_(std::exchange(other.ref_, 0)) {}

    LuaRef& operator=(LuaRef&& other) noexcept;

    ~LuaRef();

    [[nodiscard]] static LuaRef take(lua_State* state);

    [[nodiscard]] static LuaRef of(lua_State* state, std::int32_t index);

    [[nodiscard]] bool valid() const noexcept { return state_ != nullptr; }

    void push() const;

    void push(LuaArgs& lua) const;

    [[nodiscard]] lua_State* state() const noexcept { return state_; }

    void release();

private:
    LuaRef(lua_State* state, std::int32_t ref) : state_(state), ref_(ref) {}

    lua_State* state_ = nullptr;
    std::int32_t ref_ = 0;
};
} // namespace basalt::wire::lua
