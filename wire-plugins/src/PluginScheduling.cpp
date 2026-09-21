#include <basalt/wire/plugins/PluginScheduling.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace basalt::wire::plugins {
namespace {
constexpr std::int64_t kMaxTicks = 1LL << 30;

[[nodiscard]] std::int32_t ticksOf(const std::int64_t written, const std::int64_t least) {
    return static_cast<std::int32_t>(std::clamp(written, least, kMaxTicks));
}
} // namespace

void pushCancelHandle(lua::LuaArgs& lua, PluginHandle handle) {
    const auto held = std::make_shared<PluginHandle>(std::move(handle));
    lua.pushTable();
    lua::pushHostFunction(lua, "cancel", [held](lua::LuaArgs&) {
        held->remove();
        return 0;
    });
    lua.setField("cancel");
}

void installScheduling(lua::LuaSandbox& sandbox, lua::LuaArgs& lua, const SchedulingHost& host) {
    const auto shared = std::make_shared<const SchedulingHost>(host);
    lua::installWaitingFunction(sandbox, "wait", [shared](lua::LuaArgs& args, lua::LuaCall::Resume resume) {
        shared->wait(ticksOf(args.integerOr(1, 1), 1), std::move(resume));
    });
    lua::pushHostFunction(lua, "later", [shared](lua::LuaArgs& args) {
        const std::int32_t ticks = ticksOf(args.checkInteger(1), 0);
        if (!args.isFunction(2)) {
            return args.fail("later(ticks, fn) needs a function");
        }
        pushCancelHandle(args, shared->later(ticks, std::make_shared<lua::LuaRef>(lua::LuaRef::of(args.state(), 2))));
        return 1;
    });
    lua.setGlobal("later");
    lua::pushHostFunction(lua, "every", [shared](lua::LuaArgs& args) {
        const bool three = args.count() >= 3;
        const std::int32_t delay = ticksOf(args.checkInteger(1), 0);
        const std::int32_t period = ticksOf(three ? args.checkInteger(2) : args.checkInteger(1), 1);
        const std::int32_t at = three ? 3 : 2;
        if (!args.isFunction(at)) {
            return args.fail("every(period, fn) needs a function");
        }
        pushCancelHandle(
            args, shared->every(delay, period, std::make_shared<lua::LuaRef>(lua::LuaRef::of(args.state(), at))));
        return 1;
    });
    lua.setGlobal("every");
}
} // namespace basalt::wire::plugins
