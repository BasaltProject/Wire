#include <basalt/wire/plugins/PluginToml.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::wire::plugins {

void pushTomlValue(lua::LuaArgs& lua, const common::toml::TomlValue& value) {
    if (const auto* text = value.get<std::string>(); text != nullptr) {
        lua.pushString(*text);
    } else if (const auto* whole = value.get<std::int64_t>(); whole != nullptr) {
        lua.pushInteger(*whole);
    } else if (const auto* real = value.get<double>(); real != nullptr) {
        lua.pushNumber(*real);
    } else if (const auto* flag = value.get<bool>(); flag != nullptr) {
        lua.pushBoolean(*flag);
    } else if (const auto* list = value.get<std::vector<common::toml::TomlValue>>(); list != nullptr) {
        lua.pushTable();
        std::int64_t position = 1;
        for (const auto& element : *list) {
            pushTomlValue(lua, element);
            lua.setIndex(position++);
        }
    } else if (const auto* nested = value.get<common::toml::TomlTable>(); nested != nullptr) {
        pushToml(lua, *nested);
    } else {
        lua.pushNil();
    }
}

void pushToml(lua::LuaArgs& lua, const common::toml::TomlTable& table) {
    lua.pushTable();
    for (const std::string& key : table.keys()) {
        const auto* value = table.find(key);
        if (value == nullptr) {
            continue;
        }
        pushTomlValue(lua, *value);
        lua.setField(key);
        if (key.find('-') != std::string::npos) {
            std::string underscored = key;
            std::ranges::replace(underscored, '-', '_');
            pushTomlValue(lua, *value);
            lua.setField(underscored);
        }
    }
}

} // namespace basalt::wire::plugins
