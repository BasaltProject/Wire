#pragma once

#include <basalt/wire/common/json/Json.hpp>

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>

namespace basalt::wire::text {

/// A flat translation-key to format-string table, as parsed from `assets/minecraft/lang/en_us.json`.
class LangTable {
public:
    LangTable() = default;

    explicit LangTable(std::unordered_map<std::string, std::string> entries) : entries_(std::move(entries)) {}

    /// Mirrors vanilla's `Language.getOrDefault`: a missing key falls back to what the caller has.
    [[nodiscard]] std::string_view getOrDefault(std::string_view key, std::string_view fallback) const;

    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

private:
    std::unordered_map<std::string, std::string> entries_;
};

/// Parses a vanilla language file: one flat object of translation key to format string.
[[nodiscard]] std::expected<LangTable, common::JsonError> parseLangTable(std::string_view json);

} // namespace basalt::wire::text
