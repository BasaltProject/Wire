#include <basalt/wire/lua/LuaSandbox.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace basalt::wire::lua {
namespace {
constexpr int kHookStep = 1000;

constexpr const char* kSandboxKey = "basalt.sandbox";

constexpr std::string_view kRemovedGlobals[] = {"io", "debug", "dofile", "loadfile", "collectgarbage"};
constexpr std::string_view kRemovedOs[] = {"execute", "exit", "getenv", "remove", "rename", "tmpname", "setlocale"};
} // namespace

struct SandboxAccess {
    [[nodiscard]] static LuaSandbox* of(lua_State* state) {
        lua_getfield(state, LUA_REGISTRYINDEX, kSandboxKey);
        auto* sandbox = static_cast<LuaSandbox*>(lua_touserdata(state, -1));
        lua_pop(state, 1);
        return sandbox;
    }

    static void* reallocate(void* opaque, void* pointer, std::size_t oldSize, std::size_t newSize) {
        auto* sandbox = static_cast<LuaSandbox*>(opaque);
        const std::size_t previous = pointer == nullptr ? 0 : oldSize;
        if (newSize == 0) {
            std::free(pointer);
            sandbox->memoryUsed_.fetch_sub(previous, std::memory_order_relaxed);
            return nullptr;
        }
        const std::size_t used = sandbox->memoryUsed_.load(std::memory_order_relaxed);
        if (newSize > previous && used + (newSize - previous) > sandbox->memoryCeiling_) {
            return nullptr;
        }
        void* moved = std::realloc(pointer, newSize);
        if (moved == nullptr) {
            return nullptr;
        }
        sandbox->memoryUsed_.fetch_add(newSize - previous, std::memory_order_relaxed);
        return moved;
    }

    static void countHook(lua_State* state, lua_Debug*) {
        LuaSandbox* sandbox = of(state);
        if (sandbox == nullptr) {
            return;
        }
        if (sandbox->interrupted_.load(std::memory_order_relaxed)) {
            luaL_error(state, "%s", failureMessages::kInterrupted.data());
            return;
        }
        const std::int64_t left = sandbox->budget_.fetch_sub(kHookStep, std::memory_order_relaxed) - kHookStep;
        if (left <= 0) {
            luaL_error(state, "%s", failureMessages::kBudget.data());
        }
    }

    static int print(lua_State* state) {
        LuaSandbox* sandbox = of(state);
        const int count = lua_gettop(state);
        std::string line;
        for (int index = 1; index <= count; ++index) {
            if (index > 1) {
                line += '\t';
            }
            std::size_t length = 0;
            const char* text = luaL_tolstring(state, index, &length);
            line.append(text, length);
            lua_pop(state, 1);
        }
        if (sandbox != nullptr && sandbox->printLine_) {
            sandbox->printLine_(line);
        }
        return 0;
    }

    static int textOnlyLoad(lua_State* state) {
        const int count = lua_gettop(state);
        lua_pushvalue(state, lua_upvalueindex(1));
        lua_pushvalue(state, 1);
        if (count >= 2) {
            lua_pushvalue(state, 2);
        } else {
            lua_pushnil(state);
        }
        lua_pushliteral(state, "t");
        if (count >= 4) {
            lua_pushvalue(state, 4);
            lua_call(state, 4, LUA_MULTRET);
        } else {
            lua_call(state, 3, LUA_MULTRET);
        }
        return lua_gettop(state) - count;
    }

    static int require(lua_State* state) {
        LuaSandbox* sandbox = of(state);
        const char* name = luaL_checkstring(state, 1);
        lua_getfield(state, LUA_REGISTRYINDEX, "basalt.loaded");
        lua_getfield(state, -1, name);
        if (!lua_isnil(state, -1)) {
            return 1;
        }
        lua_pop(state, 1);

        if (sandbox == nullptr || !sandbox->sources_) {
            return luaL_error(state, "module '%s' not found", name);
        }
        const std::optional<std::vector<std::uint8_t>> source = sandbox->sources_(name);
        if (!source.has_value()) {
            return luaL_error(state, "module '%s' not found", name);
        }
        const std::string chunkName = std::string{"@"} + name;
        if (luaL_loadbufferx(state, reinterpret_cast<const char*>(source->data()), source->size(), chunkName.c_str(),
                             sandbox->bytecodeAllowed_ ? "bt" : "t") != LUA_OK) {
            return lua_error(state);
        }
        lua_pushstring(state, name);
        lua_call(state, 1, 1);
        if (lua_isnil(state, -1)) {
            lua_pop(state, 1);
            lua_pushboolean(state, 1);
        }
        lua_pushvalue(state, -1);
        lua_setfield(state, -3, name);
        return 1;
    }

    static int writer(lua_State*, const void* data, std::size_t size, void* opaque) {
        auto* target = static_cast<std::vector<std::uint8_t>*>(opaque);
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        target->insert(target->end(), bytes, bytes + size);
        return 0;
    }

    static void installInto(LuaSandbox& sandbox, std::uint64_t seed) { sandbox.install(seed); }
};

LuaSandbox::LuaSandbox(std::string name, LuaLimits limits, SourceFinder sources, PrintSink printLine)
    : name_(std::move(name)), limits_(limits), sources_(std::move(sources)), printLine_(std::move(printLine)),
      memoryCeiling_(limits.memoryBytes) {
    budget_.store(limits.instructionsPerTurn, std::memory_order_relaxed);
}

LuaSandbox::~LuaSandbox() {
    if (state_ != nullptr) {
        lua_close(state_);
    }
}

std::unique_ptr<LuaSandbox> LuaSandbox::create(std::string name, LuaLimits limits, std::uint64_t seed,
                                               SourceFinder sources, PrintSink printLine) {
    std::unique_ptr<LuaSandbox> sandbox{
        new LuaSandbox{std::move(name), limits, std::move(sources), std::move(printLine)}};
    sandbox->state_ =
        lua_newstate(&SandboxAccess::reallocate, sandbox.get(), static_cast<unsigned int>(seed & 0xFFFFFFFFULL));
    if (sandbox->state_ == nullptr) {
        return nullptr;
    }
    SandboxAccess::installInto(*sandbox, seed);
    return sandbox;
}

void LuaSandbox::install(std::uint64_t seed) {
    lua_pushlightuserdata(state_, this);
    lua_setfield(state_, LUA_REGISTRYINDEX, kSandboxKey);
    luaL_openlibs(state_);
    lua_newtable(state_);
    lua_setfield(state_, LUA_REGISTRYINDEX, "basalt.loaded");
    strip();
    lua_getglobal(state_, "math");
    if (lua_istable(state_, -1)) {
        lua_getfield(state_, -1, "randomseed");
        if (lua_isfunction(state_, -1)) {
            lua_pushinteger(state_, static_cast<lua_Integer>(seed));
            lua_call(state_, 1, 0);
        } else {
            lua_pop(state_, 1);
        }
    }
    lua_pop(state_, 1);
    lua_sethook(state_, &SandboxAccess::countHook, LUA_MASKCOUNT, kHookStep);
}

void LuaSandbox::strip() {
    for (const std::string_view removed : kRemovedGlobals) {
        lua_pushnil(state_);
        lua_setglobal(state_, std::string{removed}.c_str());
    }
    lua_getglobal(state_, "os");
    if (lua_istable(state_, -1)) {
        for (const std::string_view removed : kRemovedOs) {
            lua_pushnil(state_);
            lua_setfield(state_, -2, std::string{removed}.c_str());
        }
    }
    lua_pop(state_, 1);

    lua_pushnil(state_);
    lua_setglobal(state_, "package");

    lua_pushcfunction(state_, &SandboxAccess::print);
    lua_setglobal(state_, "print");
    lua_pushcfunction(state_, &SandboxAccess::require);
    lua_setglobal(state_, "require");

    lua_getglobal(state_, "load");
    lua_pushcclosure(state_, &SandboxAccess::textOnlyLoad, 1);
    lua_setglobal(state_, "load");
}

LuaFailure LuaSandbox::failureFromStack() {
    std::size_t length = 0;
    const char* text = lua_tolstring(state_, -1, &length);
    LuaFailure failure{.plugin = name_, .message = text == nullptr ? "error" : std::string{text, length}};
    lua_pop(state_, 1);
    return failure;
}

std::expected<LuaChunk, LuaFailure> LuaSandbox::compile(std::string_view source, std::string chunkName) {
    const std::string named = "@" + chunkName;
    if (luaL_loadbufferx(state_, source.data(), source.size(), named.c_str(), "t") != LUA_OK) {
        return std::unexpected(failureFromStack());
    }
    LuaChunk chunk{.chunkName = std::move(chunkName), .bytecode = {}};
    lua_dump(state_, &SandboxAccess::writer, &chunk.bytecode, 0);
    lua_pop(state_, 1);
    return chunk;
}

std::expected<void, LuaFailure> LuaSandbox::bind(const LuaChunk& chunk) {
    const std::string named = "@" + chunk.chunkName;
    if (luaL_loadbufferx(state_, reinterpret_cast<const char*>(chunk.bytecode.data()), chunk.bytecode.size(),
                         named.c_str(), "b") != LUA_OK) {
        return std::unexpected(failureFromStack());
    }
    return {};
}

std::expected<void, LuaFailure> LuaSandbox::run(const LuaChunk& chunk) {
    if (const std::expected<void, LuaFailure> loaded = bind(chunk); !loaded.has_value()) {
        return loaded;
    }
    refill();
    if (lua_pcall(state_, 0, 0, 0) != LUA_OK) {
        return std::unexpected(failureFromStack());
    }
    return {};
}

void LuaSandbox::refill() {
    interrupted_.store(false, std::memory_order_relaxed);
    budget_.store(limits_.instructionsPerTurn, std::memory_order_relaxed);
}

void LuaSandbox::interrupt() { interrupted_.store(true, std::memory_order_relaxed); }

void LuaSandbox::resetMemoryCount() { memoryUsed_.store(0, std::memory_order_relaxed); }
} // namespace basalt::wire::lua
