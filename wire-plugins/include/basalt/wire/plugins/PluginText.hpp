#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::plugins {

/// A plugin's `lang/<locale>.toml` files: `@key` names a line and `{name}` takes a field.
class PluginText {
public:
    PluginText(std::string folder, std::string defaultLocale)
        : folder_(std::move(folder)), defaultLocale_(std::move(defaultLocale)) {}

    void load();

    using Fields = std::function<std::optional<std::string>(std::string_view name)>;

    [[nodiscard]] std::string resolve(std::string_view message, const Fields& fields = {}) const;

    [[nodiscard]] std::optional<std::string> translation(std::string_view key) const;

    /// The line without copying it, or null; it stays valid until the next load().
    [[nodiscard]] const std::string* find(std::string_view key) const;

    [[nodiscard]] std::size_t localeCount() const noexcept { return locales_.size(); }

private:
    std::string folder_;
    std::string defaultLocale_;
    std::map<std::string, std::map<std::string, std::string, std::less<>>, std::less<>> locales_;
};

} // namespace basalt::wire::plugins
