#pragma once

#include <basalt/wire/lua/LuaArgs.hpp>
#include <basalt/wire/lua/LuaRef.hpp>
#include <basalt/wire/lua/LuaSandbox.hpp>

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
class LuaCall : public std::enable_shared_from_this<LuaCall> {
public:
    using Done = std::function<void(std::optional<LuaFailure>)>;

    using Resume = std::function<void()>;

    using Arrange = std::function<void(Resume)>;

    /// Like Arrange, but sees the arguments the script waited with.
    using ArrangeWith = std::function<void(LuaArgs&, Resume)>;

    LuaCall(const LuaCall&) = delete;
    LuaCall& operator=(const LuaCall&) = delete;
    ~LuaCall();

    [[nodiscard]] static std::shared_ptr<LuaCall> start(LuaSandbox& sandbox, std::int32_t argumentCount, Done onDone);

    [[nodiscard]] bool finished() const noexcept { return finished_; }

    [[nodiscard]] bool paused() const noexcept { return !finished_ && arranged_; }

private:
    explicit LuaCall(LuaSandbox& sandbox, lua_State* thread, std::int32_t threadRef, Done onDone);

    friend struct CallAccess;

    void step(std::int32_t argumentCount);

    LuaSandbox* sandbox_;
    lua_State* thread_;
    /// The thread's slot in the registry, which is what keeps it from being collected mid-pause.
    std::int32_t threadRef_;
    Done onDone_;
    bool finished_ = false;
    bool arranged_ = false;
};

void installHostFunction(LuaSandbox& sandbox, const std::string& name, std::function<std::int32_t(LuaArgs&)> body);

void installWaitingFunction(LuaSandbox& sandbox, const std::string& name, LuaCall::Arrange arrange);

void installWaitingFunction(LuaSandbox& sandbox, const std::string& name, LuaCall::ArrangeWith arrange);

void pushHostFunction(LuaArgs& lua, const std::string& name, std::function<std::int32_t(LuaArgs&)> body);

[[nodiscard]] std::expected<std::int32_t, LuaFailure>
callSync(LuaSandbox& sandbox, const LuaRef& function, const std::function<std::int32_t(LuaArgs&)>& pushArguments,
         std::int32_t resultCount);
} // namespace basalt::wire::lua
