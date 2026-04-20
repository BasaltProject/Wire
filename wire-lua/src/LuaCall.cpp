#include <basalt/wire/lua/LuaCall.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstdint>
#include <utility>

namespace basalt::wire::lua {
namespace {
constexpr const char* kCallsKey = "basalt.calls";

struct HostBody {
    std::function<std::int32_t(LuaArgs&)> immediate;
    LuaCall::Arrange arrange;
    std::string name;
};

int releaseHost(lua_State* state) {
    auto* body = static_cast<HostBody*>(lua_touserdata(state, 1));
    body->~HostBody();
    return 0;
}

[[nodiscard]] HostBody* bodyOf(lua_State* state) {
    return static_cast<HostBody*>(lua_touserdata(state, lua_upvalueindex(1)));
}

[[nodiscard]] HostBody* newBody(lua_State* state) {
    void* memory = lua_newuserdatauv(state, sizeof(HostBody), 0);
    auto* body = new (memory) HostBody{};
    if (luaL_newmetatable(state, "basalt.hostbody") != 0) {
        lua_pushcfunction(state, &releaseHost);
        lua_setfield(state, -2, "__gc");
    }
    lua_setmetatable(state, -2);
    return body;
}
} // namespace

struct CallAccess {
    [[nodiscard]] static LuaCall* of(lua_State* thread) {
        lua_getfield(thread, LUA_REGISTRYINDEX, kCallsKey);
        lua_pushthread(thread);
        lua_rawget(thread, -2);
        auto* call = static_cast<LuaCall*>(lua_touserdata(thread, -1));
        lua_pop(thread, 2);
        return call;
    }

    static void remember(lua_State* thread, LuaCall* call) {
        lua_getfield(thread, LUA_REGISTRYINDEX, kCallsKey);
        lua_pushthread(thread);
        lua_pushlightuserdata(thread, call);
        lua_rawset(thread, -3);
        lua_pop(thread, 1);
    }

    static void forget(lua_State* thread) {
        lua_getfield(thread, LUA_REGISTRYINDEX, kCallsKey);
        lua_pushthread(thread);
        lua_pushnil(thread);
        lua_rawset(thread, -3);
        lua_pop(thread, 1);
    }

    static int immediate(lua_State* state) {
        LuaArgs args{state};
        return bodyOf(state)->immediate(args);
    }

    static int afterWait(lua_State*, int, lua_KContext) { return 0; }

    static int waiting(lua_State* thread) {
        HostBody* body = bodyOf(thread);
        if (lua_isyieldable(thread) == 0) {
            return luaL_error(thread, "'%s' cannot be used from here: it needs to be able to wait", body->name.c_str());
        }
        LuaCall* call = of(thread);
        if (call == nullptr) {
            return luaL_error(thread, "'%s' cannot be used from here: it needs to be able to wait", body->name.c_str());
        }
        call->arranged_ = true;
        body->arrange([held = call->shared_from_this()] {
            if (!held->finished_) {
                held->arranged_ = false;
                held->step(0);
            }
        });
        return lua_yieldk(thread, 0, 0, &afterWait);
    }
};

LuaCall::LuaCall(LuaSandbox& sandbox, lua_State* thread, std::int32_t threadRef, Done onDone)
    : sandbox_(&sandbox), thread_(thread), threadRef_(threadRef), onDone_(std::move(onDone)) {}

LuaCall::~LuaCall() {
    if (thread_ != nullptr) {
        luaL_unref(sandbox_->state(), LUA_REGISTRYINDEX, threadRef_);
    }
}

std::shared_ptr<LuaCall> LuaCall::start(LuaSandbox& sandbox, std::int32_t argumentCount, Done onDone) {
    lua_State* main = sandbox.state();
    lua_getfield(main, LUA_REGISTRYINDEX, kCallsKey);
    if (lua_isnil(main, -1)) {
        lua_pop(main, 1);
        lua_newtable(main);
        lua_newtable(main);
        lua_pushliteral(main, "k");
        lua_setfield(main, -2, "__mode");
        lua_setmetatable(main, -2);
        lua_setfield(main, LUA_REGISTRYINDEX, kCallsKey);
    } else {
        lua_pop(main, 1);
    }

    lua_State* thread = lua_newthread(main);
    const std::int32_t threadRef = luaL_ref(main, LUA_REGISTRYINDEX);
    lua_xmove(main, thread, argumentCount + 1);

    std::shared_ptr<LuaCall> call{new LuaCall{sandbox, thread, threadRef, std::move(onDone)}};
    CallAccess::remember(thread, call.get());
    sandbox.refill();
    call->step(argumentCount);
    return call;
}

void LuaCall::step(std::int32_t argumentCount) {
    int results = 0;
    const int status = lua_resume(thread_, sandbox_->state(), argumentCount, &results);
    if (status == LUA_YIELD) {
        return;
    }
    finished_ = true;
    std::optional<LuaFailure> failure;
    if (status != LUA_OK) {
        std::size_t length = 0;
        const char* text = lua_tolstring(thread_, -1, &length);
        failure =
            LuaFailure{.plugin = sandbox_->name(), .message = text == nullptr ? "error" : std::string{text, length}};
        lua_pop(thread_, 1);
    }
    CallAccess::forget(thread_);
    if (onDone_) {
        const Done done = std::move(onDone_);
        onDone_ = nullptr;
        done(failure);
    }
}

namespace {
void installBody(LuaSandbox& sandbox, const std::string& name, HostBody built, lua_CFunction entry) {
    lua_State* state = sandbox.state();
    HostBody* body = newBody(state);
    *body = std::move(built);
    body->name = name;
    lua_pushcclosure(state, entry, 1);
    lua_setglobal(state, name.c_str());
}
} // namespace

void pushHostFunction(LuaArgs& lua, const std::string& name, std::function<std::int32_t(LuaArgs&)> body) {
    lua_State* state = lua.state();
    HostBody* held = newBody(state);
    *held = HostBody{.immediate = std::move(body), .arrange = {}, .name = name};
    lua_pushcclosure(state, &CallAccess::immediate, 1);
}

void installHostFunction(LuaSandbox& sandbox, const std::string& name, std::function<std::int32_t(LuaArgs&)> body) {
    installBody(sandbox, name, HostBody{.immediate = std::move(body), .arrange = {}, .name = name},
                &CallAccess::immediate);
}

void installWaitingFunction(LuaSandbox& sandbox, const std::string& name, LuaCall::Arrange arrange) {
    installBody(sandbox, name, HostBody{.immediate = {}, .arrange = std::move(arrange), .name = name},
                &CallAccess::waiting);
}

std::expected<std::int32_t, LuaFailure> callSync(LuaSandbox& sandbox, const LuaRef& function,
                                                 const std::function<std::int32_t(LuaArgs&)>& pushArguments,
                                                 std::int32_t resultCount) {
    lua_State* state = sandbox.state();
    if (!function.valid()) {
        return std::unexpected(LuaFailure{.plugin = sandbox.name(), .message = "there is no function to call"});
    }
    const int before = lua_gettop(state);
    function.push();
    LuaArgs lua{state};
    const std::int32_t argumentCount = pushArguments ? pushArguments(lua) : 0;
    if (lua_pcall(state, argumentCount, resultCount, 0) != LUA_OK) {
        const LuaFailure failure = sandbox.failureFromStack();
        lua_settop(state, before);
        return std::unexpected(failure);
    }
    return lua_gettop(state) - before;
}
} // namespace basalt::wire::lua
