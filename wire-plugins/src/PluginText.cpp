#include <basalt/wire/common/toml/Toml.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/plugins/PluginText.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace basalt::wire::plugins {
namespace {
using common::toml::TomlTable;
using common::toml::TomlValue;

void flatten(const TomlTable& table, const std::string& prefix, std::map<std::string, std::string, std::less<>>& into) {
    for (const std::string& key : table.keys()) {
        const TomlValue* value = table.find(key);
        if (value == nullptr) {
            continue;
        }
        if (const std::string* text = value->get<std::string>(); text != nullptr) {
            into.insert_or_assign(prefix + key, *text);
            continue;
        }
        if (const TomlTable* nested = value->get<TomlTable>(); nested != nullptr) {
            flatten(*nested, prefix + key + ".", into);
        }
    }
}

[[nodiscard]] std::string lowered(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char character : text) {
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return out;
}
} // namespace

void PluginText::load() {
    const std::string directory = folder_ + "/lang";
    if (!io::fileIO::isDirectory(directory)) {
        return;
    }
    std::vector<std::string> names = io::fileIO::list(directory);
    std::sort(names.begin(), names.end());
    for (const std::string& entry : names) {
        if (!entry.ends_with(".toml")) {
            continue;
        }
        const std::optional<std::string> text = io::fileIO::readText(directory + "/" + entry);
        if (!text.has_value()) {
            continue;
        }
        const std::expected<TomlTable, common::toml::TomlError> table = common::toml::parse(*text);
        if (!table.has_value()) {
            continue;
        }
        std::map<std::string, std::string, std::less<>> flat;
        flatten(*table, "", flat);
        locales_.insert_or_assign(lowered(entry.substr(0, entry.size() - 5)), std::move(flat));
    }
}

std::optional<std::string> PluginText::translation(std::string_view key) const {
    if (const auto preferred = locales_.find(defaultLocale_); preferred != locales_.end()) {
        if (const auto found = preferred->second.find(key); found != preferred->second.end()) {
            return found->second;
        }
    }
    for (const auto& [locale, entries] : locales_) {
        if (const auto found = entries.find(key); found != entries.end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

std::string PluginText::resolve(std::string_view message, lua::LuaArgs* lua, std::int32_t arguments) const {
    std::string text;
    if (message.starts_with('@')) {
        const std::string_view key = message.substr(1);
        text = translation(key).value_or(std::string{key});
    } else {
        text = std::string{message};
    }
    if (text.find('{') == std::string::npos || lua == nullptr || arguments == 0 || !lua->isTable(arguments)) {
        return text;
    }

    std::string out;
    out.reserve(text.size());
    std::size_t index = 0;
    while (index < text.size()) {
        if (text[index] != '{') {
            out += text[index];
            ++index;
            continue;
        }
        const std::size_t close = text.find('}', index);
        if (close == std::string::npos) {
            out += text.substr(index);
            break;
        }
        const std::string name = text.substr(index + 1, close - index - 1);
        const std::optional<std::string> value = lua->fieldAsText(arguments, name);
        out += value.value_or("{" + name + "}");
        index = close + 1;
    }
    return out;
}
} // namespace basalt::wire::plugins
