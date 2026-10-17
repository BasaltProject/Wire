#pragma once

#include <basalt/wire/lua/LuaArgs.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::plugins {

/// A plugin's `lang/<locale>.toml` files: `@key` names a line and `{name}` takes a field of the table of arguments.
class PluginText {
public:
    PluginText(std::string folder, std::string defaultLocale)
        : folder_(std::move(folder)), defaultLocale_(std::move(defaultLocale)) {}

    void load();

    [[nodiscard]] std::string resolve(std::string_view message, lua::LuaArgs* lua, std::int32_t arguments) const;

    [[nodiscard]] std::optional<std::string> translation(std::string_view key) const;

    [[nodiscard]] std::size_t localeCount() const noexcept { return locales_.size(); }

private:
    std::string folder_;
    std::string defaultLocale_;
    std::map<std::string, std::map<std::string, std::string, std::less<>>, std::less<>> locales_;
};

} // namespace basalt::wire::plugins
