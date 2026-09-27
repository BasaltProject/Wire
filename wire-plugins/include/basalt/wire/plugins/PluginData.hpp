#pragma once

#include <basalt/wire/lua/LuaArgs.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::plugins {

inline constexpr std::size_t kMaxDataKeyBytes = 128;
inline constexpr std::size_t kMaxDataValueBytes = 64 * 1024;

/// The host side of one data scope (`store` for the server, `player.data` for a player): reads, writes and the ranking.
struct DataScopeHost {
    /// A reason to refuse every access (a missing capability), or nullptr.
    std::function<const char*()> problem;
    std::function<std::optional<std::string>(std::string_view key)> get;
    /// Stores or, for nullopt, removes a value; returns the reason it was refused, if it was.
    std::function<std::optional<std::string>(std::string_view key, const std::optional<std::string>& value)> set;
    /// The plugin's highest values for a key across every scope as (uuid, stored value); empty disables `top`.
    std::function<std::vector<std::pair<std::string, std::string>>(std::string_view key, std::int32_t limit)> top;
    std::function<std::string(const std::string& uuid)> nameOf;
};

/// A table where `t.key` reads, `t.key = value` writes (nil deletes) and, when the host has it, `t.top(key, n)` ranks.
void pushDataScope(lua::LuaArgs& lua, DataScopeHost host);

} // namespace basalt::wire::plugins
