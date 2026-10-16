#include <basalt/wire/lua/LuaArgs.hpp>
#include <basalt/wire/lua/LuaRef.hpp>

#include <cstdint>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace basalt::wire::lua {
LuaRef& LuaRef::operator=(LuaRef&& other) noexcept {
    if (this != &other) {
        release();
        state_ = std::exchange(other.state_, nullptr);
        ref_ = std::exchange(other.ref_, 0);
    }
    return *this;
}

LuaRef::~LuaRef() { release(); }

LuaRef LuaRef::take(lua_State* state) {
    if (state == nullptr) {
        return {};
    }
    return LuaRef{state, luaL_ref(state, LUA_REGISTRYINDEX)};
}

LuaRef LuaRef::of(lua_State* state, std::int32_t index) {
    if (state == nullptr) {
        return {};
    }
    lua_pushvalue(state, index);
    return take(state);
}

void LuaRef::push() const {
    if (state_ == nullptr) {
        return;
    }
    lua_rawgeti(state_, LUA_REGISTRYINDEX, ref_);
}

void LuaRef::push(LuaArgs& lua) const {
    if (state_ == nullptr) {
        lua.pushNil();
        return;
    }
    push();
}

void LuaRef::release() {
    if (state_ == nullptr) {
        return;
    }
    luaL_unref(state_, LUA_REGISTRYINDEX, ref_);
    state_ = nullptr;
    ref_ = 0;
}
} // namespace basalt::wire::lua
