#include <basalt/wire/config/MessageCatalog.hpp>

#include <algorithm>

namespace basalt::wire::config {

std::expected<MessageCatalog, std::string> MessageCatalog::parse(const common::toml::TomlTable& root,
                                                                 std::string_view path,
                                                                 std::span<const MessageSection> sections) {
    MessageCatalog catalog;
    for (const MessageSection& section : sections) {
        auto& texts = catalog.sections_[std::string{section.name}];
        const common::toml::TomlTable* table = root.table(section.name);
        if (table == nullptr) {
            continue;
        }
        for (const std::string& key : table->keys()) {
            if (std::ranges::find(section.keys, std::string_view{key}) == section.keys.end()) {
                return std::unexpected(std::string{path} + ": " + std::string{section.name} + "." + key +
                                       " is not a message this product sends");
            }
            const std::optional<std::string_view> text = table->string(key);
            if (!text.has_value()) {
                return std::unexpected(std::string{path} + ": " + std::string{section.name} + "." + key +
                                       " must be a string");
            }
            texts.emplace(key, std::string{*text});
        }
    }
    return catalog;
}

const std::string* MessageCatalog::find(std::string_view section, std::string_view key) const {
    const auto texts = sections_.find(section);
    if (texts == sections_.end()) {
        return nullptr;
    }
    const auto found = texts->second.find(key);
    return found == texts->second.end() || found->second.empty() ? nullptr : &found->second;
}

const std::map<std::string, std::string, std::less<>>& MessageCatalog::section(std::string_view name) const {
    static const std::map<std::string, std::string, std::less<>> kNone;
    const auto texts = sections_.find(name);
    return texts == sections_.end() ? kNone : texts->second;
}

} // namespace basalt::wire::config
