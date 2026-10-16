#include <basalt/wire/common/toml/Toml.hpp>
#include <basalt/wire/plugins/PluginManifest.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>

namespace basalt::wire::plugins {
namespace {
using wire::common::toml::TomlTable;

constexpr std::string_view kDefaultMain = "src/main.lua";

[[nodiscard]] bool isUsableId(std::string_view id) {
    if (id.empty() || id.front() < 'a' || id.front() > 'z') {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](const char character) {
        return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '_';
    });
}

[[nodiscard]] std::vector<std::string_view> splitVersion(std::string_view version) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (std::size_t index = 0; index <= version.size(); ++index) {
        if (index == version.size() || version[index] == '.' || version[index] == '-') {
            parts.push_back(version.substr(start, index - start));
            start = index + 1;
        }
    }
    return parts;
}

[[nodiscard]] std::int64_t numberOf(std::string_view part) {
    std::int64_t value = 0;
    if (std::from_chars(part.data(), part.data() + part.size(), value).ec != std::errc{}) {
        return 0;
    }
    return value;
}

[[nodiscard]] std::string_view trimmed(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] PluginCapabilities capabilitiesOf(const TomlTable* table) {
    if (table == nullptr) {
        return PluginCapabilities{};
    }
    return PluginCapabilities{.storage = table->boolean("storage").value_or(false),
                              .playersData = table->boolean("players_data").value_or(false),
                              .commands = table->boolean("commands").value_or(false),
                              .files = table->stringList("files"),
                              .network = table->stringList("network"),
                              .packets = table->boolean("packets").value_or(false)};
}

[[nodiscard]] PluginExports exportsOf(const TomlTable* table) {
    if (table == nullptr) {
        return PluginExports{};
    }
    return PluginExports{.modules = table->stringList("modules"), .services = table->stringList("services")};
}

[[nodiscard]] std::vector<PluginDependency> dependenciesOf(const TomlTable& table) {
    std::vector<PluginDependency> found;
    for (const auto& [key, hard] : {std::pair<std::string_view, bool>{"depends", true},
                                    std::pair<std::string_view, bool>{"soft-depends", false}}) {
        const TomlTable* section = table.table(key);
        if (section == nullptr) {
            continue;
        }
        for (const std::string& id : section->keys()) {
            const std::optional<std::string_view> requirement = section->string(id);
            found.push_back(
                PluginDependency{.id = id, .requirement = std::string{requirement.value_or("*")}, .hard = hard});
        }
    }
    return found;
}
} // namespace

std::string_view permissionName(PluginPermission permission) {
    switch (permission) {
    case PluginPermission::Packets:
        return "packets";
    case PluginPermission::Network:
        return "network";
    case PluginPermission::Commands:
    default:
        return "commands";
    }
}

std::optional<PluginPermission> permissionOf(std::string_view name) {
    if (name == "packets") {
        return PluginPermission::Packets;
    }
    if (name == "network") {
        return PluginPermission::Network;
    }
    if (name == "commands") {
        return PluginPermission::Commands;
    }
    return std::nullopt;
}

std::set<PluginPermission> PluginCapabilities::asks() const {
    std::set<PluginPermission> asked;
    if (packets) {
        asked.insert(PluginPermission::Packets);
    }
    if (!network.empty()) {
        asked.insert(PluginPermission::Network);
    }
    if (commands) {
        asked.insert(PluginPermission::Commands);
    }
    return asked;
}

bool PluginExports::opensEverything() const { return std::find(modules.begin(), modules.end(), "*") != modules.end(); }

bool PluginExports::exportsModule(std::string_view module) const {
    return opensEverything() || std::find(modules.begin(), modules.end(), module) != modules.end();
}

std::int32_t compareVersions(std::string_view left, std::string_view right) {
    const std::vector<std::string_view> a = splitVersion(left);
    const std::vector<std::string_view> b = splitVersion(right);
    for (std::size_t index = 0; index < std::max(a.size(), b.size()); ++index) {
        const std::int64_t x = index < a.size() ? numberOf(a[index]) : 0;
        const std::int64_t y = index < b.size() ? numberOf(b[index]) : 0;
        if (x != y) {
            return x < y ? -1 : 1;
        }
    }
    return 0;
}

bool PluginDependency::accepts(std::string_view version) const {
    const std::string_view wanted{requirement};
    if (wanted == "*") {
        return true;
    }
    if (wanted.starts_with(">=")) {
        return compareVersions(version, trimmed(wanted.substr(2))) >= 0;
    }
    if (wanted.starts_with(">")) {
        return compareVersions(version, trimmed(wanted.substr(1))) > 0;
    }
    if (wanted.starts_with("<=")) {
        return compareVersions(version, trimmed(wanted.substr(2))) <= 0;
    }
    if (wanted.starts_with("<")) {
        return compareVersions(version, trimmed(wanted.substr(1))) < 0;
    }
    return compareVersions(version, trimmed(wanted)) == 0;
}

std::vector<PluginDependency> PluginManifest::hardDependencies() const {
    std::vector<PluginDependency> hard;
    for (const PluginDependency& dependency : dependencies) {
        if (dependency.hard) {
            hard.push_back(dependency);
        }
    }
    return hard;
}

std::expected<PluginManifest, ManifestError> PluginManifest::parse(std::string_view text) {
    const std::expected<TomlTable, wire::common::toml::TomlError> table = wire::common::toml::parse(text);
    if (!table.has_value()) {
        return std::unexpected(ManifestError{std::string{"plugin.toml is not valid TOML: "} +
                                             std::string{wire::common::toml::tomlErrorMessage(table.error())}});
    }
    const std::optional<std::string_view> id = table->string("id");
    if (!id.has_value()) {
        return std::unexpected(ManifestError{"plugin.toml has no 'id'"});
    }
    if (!isUsableId(*id)) {
        return std::unexpected(
            ManifestError{"'" + std::string{*id} +
                          "' is not a usable id: lowercase letters, digits and underscores, starting with a letter"});
    }
    const std::optional<std::string_view> version = table->string("version");
    if (!version.has_value()) {
        return std::unexpected(ManifestError{std::string{*id} + " has no 'version'"});
    }
    const std::optional<std::int64_t> api = table->integer("api");
    if (!api.has_value()) {
        return std::unexpected(ManifestError{std::string{*id} + " does not say which 'api' it was written for"});
    }
    const TomlTable* resources = table->table("resources");
    return PluginManifest{
        .id = std::string{*id},
        .name = std::string{table->string("name").value_or(*id)},
        .version = std::string{*version},
        .authors = table->stringList("authors"),
        .api = static_cast<std::int32_t>(*api),
        .main = std::string{table->string("main").value_or(kDefaultMain)},
        .dependencies = dependenciesOf(*table),
        .capabilities = capabilitiesOf(table->table("capabilities")),
        .exports = exportsOf(table->table("exports")),
        .privateResources = resources == nullptr ? std::vector<std::string>{} : resources->stringList("private"),
    };
}
} // namespace basalt::wire::plugins
