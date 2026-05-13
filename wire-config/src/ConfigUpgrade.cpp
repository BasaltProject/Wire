#include <basalt/wire/config/ConfigUpgrade.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <vector>

namespace basalt::wire::config {
namespace {

using wire::common::toml::TomlTable;

struct Entry {
    std::string section;
    std::string key;
    std::string comment;
    std::string line;
};

std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

bool isBareKey(std::string_view key) {
    return !key.empty() && std::ranges::all_of(key, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}

std::vector<Entry> entriesOf(std::string_view text, const std::vector<std::string>& sections) {
    std::vector<Entry> entries;
    std::string section;
    std::string comment;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        const std::string_view line = trim(text.substr(start, end - start));
        start = end + 1;
        if (line.empty()) {
            comment.clear();
        } else if (line.front() == '#') {
            comment += std::string{line} + "\n";
        } else if (line.front() == '[') {
            section = std::string{trim(line.substr(1, line.rfind(']') - 1))};
            comment.clear();
        } else {
            const std::size_t equals = line.find('=');
            const std::string_view key =
                equals == std::string_view::npos ? std::string_view{} : trim(line.substr(0, equals));
            if (isBareKey(key) && std::ranges::find(sections, section) != sections.end()) {
                entries.push_back(Entry{section, std::string{key}, comment, std::string{line}});
            }
            comment.clear();
        }
    }
    return entries;
}

bool present(const TomlTable& root, const Entry& entry) {
    const TomlTable* table = &root;
    std::size_t at = 0;
    while (at <= entry.section.size()) {
        const std::size_t dot = std::min(entry.section.find('.', at), entry.section.size());
        table = table->table(std::string_view{entry.section}.substr(at, dot - at));
        if (table == nullptr) {
            return false;
        }
        at = dot + 1;
    }
    return table->find(entry.key) != nullptr;
}

} // namespace

std::string appendMissingOptions(const UpgradeRules& rules, std::string_view path, std::string_view fileName,
                                 std::string_view defaultContents, const TomlTable& values,
                                 const wire::common::Logger& logger) {
    const auto fixed = std::ranges::find(rules.files, fileName, &UpgradableFile::file);
    if (fixed == rules.files.end()) {
        return {};
    }
    const std::optional<std::string> current = wire::io::fileIO::readText(std::string{path});
    if (!current.has_value()) {
        return {};
    }
    std::map<std::string, std::string> blocks;
    std::string added;
    for (const Entry& entry : entriesOf(defaultContents, fixed->sections)) {
        if (present(values, entry) || current->find("\n# " + entry.line) != std::string::npos) {
            continue;
        }
        blocks[entry.section] += entry.comment + "# " + entry.line + "\n";
        added += (added.empty() ? "" : ", ") + entry.section + "." + entry.key;
    }
    if (blocks.empty()) {
        return {};
    }
    std::string appended = "\n# ---- Options added in Basalt " + rules.version +
                           ", commented out with their defaults. Remove the leading # to change one. ----\n";
    for (const auto& [section, block] : blocks) {
        appended += "\n# [" + section + "]\n" + block;
    }
    wire::io::fileIO::writeText(std::string{path}, *current + (current->ends_with("\n") ? "" : "\n") + appended);
    logger.info([&path, &added] {
        return std::string{path} +
               " lacked options this version has; they are listed as comments at its end: " + added + ".";
    });
    return added;
}

} // namespace basalt::wire::config
