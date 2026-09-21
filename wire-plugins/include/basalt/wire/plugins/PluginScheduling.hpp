#pragma once

#include <basalt/wire/lua/LuaArgs.hpp>
#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/lua/LuaRef.hpp>
#include <basalt/wire/lua/LuaSandbox.hpp>
#include <basalt/wire/plugins/PluginPackets.hpp>

#include <cstdint>
#include <functional>
#include <memory>

namespace basalt::wire::plugins {

/// The host side of wait(), later() and every(); a tick is the host's own (a server tick, 50 ms on a proxy).
struct SchedulingHost {
    std::function<PluginHandle(std::int32_t ticks, std::shared_ptr<lua::LuaRef> function)> later;
    std::function<PluginHandle(std::int32_t delay, std::int32_t period, std::shared_ptr<lua::LuaRef> function)> every;
    /// Resumes the calling script after `ticks`; never calling `resume` abandons it.
    std::function<void(std::int32_t ticks, lua::LuaCall::Resume resume)> wait;
};

/// A table with one method, `cancel`, that removes what the handle registered.
void pushCancelHandle(lua::LuaArgs& lua, PluginHandle handle);

/// Installs the globals `wait`, `later` and `every`.
void installScheduling(lua::LuaSandbox& sandbox, lua::LuaArgs& lua, const SchedulingHost& host);

} // namespace basalt::wire::plugins
