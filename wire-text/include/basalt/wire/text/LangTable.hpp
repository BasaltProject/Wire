#pragma once

#include <basalt/wire/common/json/Json.hpp>

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>

namespace basalt::wire::text {

class LangTable {
public:
    LangTable() = default;

    explicit LangTable(std::unordered_map<std::string, std::string> entries) : entries_(std::move(entries)) {}

    [[nodiscard]] std::string_view getOrDefault(std::string_view key, std::string_view fallback) const;

    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

private:
    std::unordered_map<std::string, std::string> entries_;
};

[[nodiscard]] std::expected<LangTable, common::JsonError> parseLangTable(std::string_view json);

} // namespace basalt::wire::text
