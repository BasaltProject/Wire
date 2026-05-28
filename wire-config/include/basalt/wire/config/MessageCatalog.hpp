#pragma once

#include <basalt/wire/common/toml/Toml.hpp>

#include <expected>
#include <map>
#include <span>
#include <string>
#include <string_view>

namespace basalt::wire::config {

struct MessageSection {
    std::string_view name;
    std::span<const std::string_view> keys;
};

/// The texts of a messages.toml by section and key; an empty text counts as unset.
class MessageCatalog {
public:
    static constexpr std::string_view kFileName = "messages.toml";

    [[nodiscard]] static std::expected<MessageCatalog, std::string>
    parse(const common::toml::TomlTable& root, std::string_view path, std::span<const MessageSection> sections);

    /// Null when the key is absent or empty, so the caller falls back to its built-in text.
    [[nodiscard]] const std::string* find(std::string_view section, std::string_view key) const;

    [[nodiscard]] const std::map<std::string, std::string, std::less<>>& section(std::string_view name) const;

private:
    std::map<std::string, std::map<std::string, std::string, std::less<>>, std::less<>> sections_;
};

} // namespace basalt::wire::config
