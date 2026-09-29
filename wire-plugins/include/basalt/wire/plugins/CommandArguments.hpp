#pragma once

#include <basalt/wire/lua/LuaArgs.hpp>
#include <basalt/wire/plugins/CommandSpec.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace basalt::wire::plugins {

/// One argument of a plugin command as the handler sees it; monostate for an optional argument left out.
using CommandValue = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

/// Reads the words after the spec's literals into typed values; nullopt when the line does not fit the usage.
[[nodiscard]] std::optional<std::vector<CommandValue>> parseCommandWords(const CommandSpec& spec,
                                                                         std::span<const std::string> words);

void pushCommandValue(lua::LuaArgs& lua, const CommandValue& value);

} // namespace basalt::wire::plugins
