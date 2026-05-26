#pragma once

#include <basalt/wire/common/toml/Toml.hpp>
#include <basalt/wire/lua/LuaArgs.hpp>

namespace basalt::wire::plugins {

void pushToml(lua::LuaArgs& lua, const common::toml::TomlTable& table);

void pushTomlValue(lua::LuaArgs& lua, const common::toml::TomlValue& value);

} // namespace basalt::wire::plugins
