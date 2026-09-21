#pragma once

#include <basalt/wire/lua/LuaArgs.hpp>
#include <basalt/wire/lua/LuaRef.hpp>
#include <basalt/wire/plugins/EventPriority.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::plugins {

inline constexpr std::string_view kPluginEnableEvent = "plugin.enable";
inline constexpr std::string_view kPluginDisableEvent = "plugin.disable";

using ListenerValue = std::variant<bool, std::int64_t, double, std::string>;

struct ListenerOptions {
    EventPriority priority = EventPriority::Normal;
    std::optional<bool> cancelled;
    std::map<std::string, ListenerValue, std::less<>> filters;
};

/// One on() or watch() call once its arguments were read.
struct ListenerRequest {
    std::string event;
    bool observer = false;
    lua::LuaRef function;
    ListenerOptions options;
};

/// The host side of on() and watch(); a returned text is the error the plugin sees.
struct ListenerHost {
    /// The fields an event can be filtered by, or why there is no such event.
    std::function<std::expected<std::vector<std::string>, std::string>(std::string_view event)> filterFields;
    std::function<std::optional<std::string>(ListenerRequest request)> add;
    /// plugin.enable and plugin.disable.
    std::function<void(bool enable, lua::LuaRef function)> lifecycle;
    /// Events a plugin names itself ("plugin:event"); unset when the host has none.
    std::function<void(std::string name, lua::LuaRef function)> custom;
};

/// Reads the table of an on() or watch(): a priority, `cancelled` and one value per filterable field.
[[nodiscard]] std::expected<ListenerOptions, std::string>
readListenerOptions(lua::LuaArgs& lua, std::int32_t index, const std::vector<std::string>& filterFields);

/// Installs the globals `on` and `watch`.
void installListeners(lua::LuaArgs& lua, const ListenerHost& host);

} // namespace basalt::wire::plugins
