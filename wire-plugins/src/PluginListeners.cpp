#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/plugins/PluginListeners.hpp>

#include <memory>
#include <utility>

namespace basalt::wire::plugins {
namespace {
[[nodiscard]] ListenerValue valueAt(lua::LuaArgs& lua, std::int32_t index) {
    if (lua.isBoolean(index)) {
        return lua.toBoolean(index);
    }
    if (lua.isNumber(index)) {
        return lua.isInteger(index) ? ListenerValue{lua.toInteger(index)} : ListenerValue{lua.toNumber(index)};
    }
    return lua.toText(index);
}
} // namespace

std::expected<ListenerOptions, std::string> readListenerOptions(lua::LuaArgs& lua, const std::int32_t index,
                                                                const std::vector<std::string>& filterFields) {
    ListenerOptions options;
    if (index == 0 || !lua.isTable(index)) {
        return options;
    }
    lua.pushField(index, "priority");
    if (lua.isString(-1)) {
        const std::string written = lua.toText(-1);
        lua.pop(1);
        const std::optional<EventPriority> spelled = priorityOf(written);
        if (!spelled.has_value()) {
            return std::unexpected("'" + written + "' is not a priority");
        }
        options.priority = *spelled;
    } else {
        lua.pop(1);
    }
    lua.pushField(index, "cancelled");
    if (!lua.isNil(-1)) {
        options.cancelled = lua.toBoolean(-1);
    }
    lua.pop(1);
    for (const std::string& field : filterFields) {
        lua.pushField(index, field);
        if (!lua.isNil(-1)) {
            options.filters.emplace(field, valueAt(lua, lua.top()));
        }
        lua.pop(1);
    }
    return options;
}

void installListeners(lua::LuaArgs& lua, const ListenerHost& host) {
    const auto shared = std::make_shared<const ListenerHost>(host);
    for (const bool observer : {false, true}) {
        const std::string verb = observer ? "watch" : "on";
        lua::pushHostFunction(lua, verb, [shared, observer, verb](lua::LuaArgs& args) {
            const std::string name = args.checkString(1);
            const bool hasOptions = args.count() >= 3;
            const std::int32_t options = hasOptions ? 2 : 0;
            const std::int32_t at = hasOptions ? 3 : 2;
            if (!args.isFunction(at)) {
                return args.fail(verb + "(event, fn) needs a function");
            }
            lua::LuaRef function = lua::LuaRef::of(args.state(), at);
            if (name == kPluginEnableEvent || name == kPluginDisableEvent) {
                if (observer) {
                    return args.fail("'" + name + "' is about this plugin: listen to it with on(), not watch()");
                }
                shared->lifecycle(name == kPluginEnableEvent, std::move(function));
                return 0;
            }
            if (shared->custom && name.find(':') != std::string::npos) {
                if (observer) {
                    return args.fail("'" + name + "' belongs to a plugin: listen to it with on()");
                }
                shared->custom(name, std::move(function));
                return 0;
            }
            const std::expected<std::vector<std::string>, std::string> fields = shared->filterFields(name);
            if (!fields.has_value()) {
                return args.fail(fields.error());
            }
            std::expected<ListenerOptions, std::string> read = readListenerOptions(args, options, *fields);
            if (!read.has_value()) {
                return args.fail(read.error());
            }
            ListenerRequest request{
                .event = name, .observer = observer, .function = std::move(function), .options = std::move(*read)};
            if (const std::optional<std::string> refused = shared->add(std::move(request)); refused.has_value()) {
                return args.fail(*refused);
            }
            return 0;
        });
        lua.setGlobal(verb);
    }
}
} // namespace basalt::wire::plugins
