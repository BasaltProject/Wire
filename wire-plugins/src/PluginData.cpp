#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/plugins/PluginData.hpp>
#include <basalt/wire/plugins/PluginValue.hpp>

#include <memory>

namespace basalt::wire::plugins {
namespace {
using lua::LuaArgs;

[[nodiscard]] std::int32_t firstArgument(const LuaArgs& args) { return args.count() >= 1 && args.isTable(1) ? 2 : 1; }

void pushTop(LuaArgs& lua, const std::shared_ptr<const DataScopeHost>& host) {
    lua::pushHostFunction(lua, "data.top", [host](LuaArgs& args) {
        if (const char* why = host->problem ? host->problem() : nullptr; why != nullptr) {
            args.raise(why);
        }
        const std::int32_t first = firstArgument(args);
        const std::string key = args.checkString(first);
        const auto limit = static_cast<std::int32_t>(args.integerOr(first + 1, 10));
        args.pushTable();
        std::int64_t position = 1;
        for (const auto& [uuid, value] : host->top(key, limit)) {
            args.pushTable();
            args.pushString(uuid);
            args.setField("uuid");
            args.pushString(host->nameOf ? host->nameOf(uuid) : uuid);
            args.setField("name");
            pushValue(args, value);
            args.setField("value");
            args.setIndex(position++);
        }
        return 1;
    });
}
} // namespace

void pushDataScope(LuaArgs& lua, DataScopeHost host) {
    const auto shared = std::make_shared<const DataScopeHost>(std::move(host));
    lua.pushTable();
    lua.pushTable();
    lua::pushHostFunction(lua, "data.index", [shared](LuaArgs& args) {
        if (const char* why = shared->problem ? shared->problem() : nullptr; why != nullptr) {
            args.raise(why);
        }
        const std::string key = args.checkString(2);
        if (key == "top" && shared->top) {
            pushTop(args, shared);
            return 1;
        }
        const std::optional<std::string> stored = key.size() <= kMaxDataKeyBytes ? shared->get(key) : std::nullopt;
        if (stored.has_value()) {
            pushValue(args, *stored);
        } else {
            args.pushNil();
        }
        return 1;
    });
    lua.setField("__index");
    lua::pushHostFunction(lua, "data.newindex", [shared](LuaArgs& args) {
        if (const char* why = shared->problem ? shared->problem() : nullptr; why != nullptr) {
            args.raise(why);
        }
        const std::string key = args.checkString(2);
        if (key.empty() || key.size() > kMaxDataKeyBytes) {
            args.raise("a data key is 1 to 128 bytes");
        }
        std::optional<std::string> encoded;
        if (!args.isNil(3)) {
            encoded = encodeValue(args, 3);
            if (!encoded.has_value()) {
                args.raise("a stored value is a boolean, a number or a string");
            }
            if (encoded->size() > kMaxDataValueBytes) {
                args.raise("a stored value is at most 65536 bytes");
            }
        }
        if (const std::optional<std::string> refused = shared->set(key, encoded); refused.has_value()) {
            args.raise(*refused);
        }
        return 0;
    });
    lua.setField("__newindex");
    lua.setMetatable(-2);
}

} // namespace basalt::wire::plugins
