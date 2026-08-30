#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <expected>

namespace basalt::wire::text {

/// Parses `assets/minecraft/lang/en_us.json`: one flat object of key to format string. Anything that
/// is not a string is skipped rather than refused, so one odd entry does not cost the whole file.
std::expected<LangTable, common::JsonError> parseLangTable(std::string_view json) {
    const auto parsed = common::parseJson(json);
    if (!parsed) {
        return std::unexpected{parsed.error()};
    }
    const auto* root = parsed->get<common::JsonObject>();
    if (root == nullptr) {
        return std::unexpected{common::JsonError::WrongType};
    }

    std::unordered_map<std::string, std::string> entries;
    entries.reserve(root->size());
    for (std::size_t i = 0; i < root->size(); ++i) {
        if (const auto* value = root->values()[i].get<std::string>()) {
            entries.emplace(root->names()[i], *value);
        }
    }
    return LangTable{std::move(entries)};
}

} // namespace basalt::wire::text
