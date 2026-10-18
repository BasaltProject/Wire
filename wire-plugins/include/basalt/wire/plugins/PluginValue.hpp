#pragma once

#include <basalt/wire/lua/LuaArgs.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::wire::plugins {

/// A Lua boolean, number or string as the tagged text a plugin store keeps; nullopt for any other type.
[[nodiscard]] std::optional<std::string> encodeValue(lua::LuaArgs& lua, std::int32_t index);

/// Pushes what encodeValue stored; an empty text is nil and a whole number comes back as an integer.
void pushValue(lua::LuaArgs& lua, const std::string& stored);

} // namespace basalt::wire::plugins
