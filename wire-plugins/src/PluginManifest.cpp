#include <basalt/wire/common/toml/Toml.hpp>
#include <basalt/wire/plugins/PluginManifest.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <utility>

namespace basalt::wire::plugins {
namespace {
using wire::common::toml::TomlTable;

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

[[nodiscard]] std::expected<PluginCapabilities, ManifestError> capabilitiesOf(const TomlTable* table) {
    PluginCapabilities capabilities;
    if (table == nullptr) {
        return capabilities;
    }
    for (const std::string& key : table->keys()) {
        if (std::find(kKnownCapabilities.begin(), kKnownCapabilities.end(), key) == kKnownCapabilities.end()) {
            std::string known;
            for (const std::string_view name : kKnownCapabilities) {
                known += known.empty() ? std::string{name} : ", " + std::string{name};
            }
            return std::unexpected(
                ManifestError{"unknown capability '" + key + "' in [capabilities]; known: " + known});
        }
    }
    capabilities.storage = table->boolean("storage").value_or(false);
    capabilities.playersData = table->boolean("players_data").value_or(false);
    capabilities.commands = table->boolean("commands").value_or(false);
    capabilities.files = table->stringList("files");
    capabilities.network = table->stringList("network");
    capabilities.packets = table->boolean("packets").value_or(false);
    const auto declare = [&capabilities](std::string_view name, bool on) {
        if (on) {
            capabilities.declared.emplace(name);
        }
    };
    declare("storage", capabilities.storage);
    declare("players_data", capabilities.playersData);
    declare("commands", capabilities.commands);
    declare("files", !capabilities.files.empty());
    declare("network", !capabilities.network.empty());
    declare("packets", capabilities.packets);
    declare("worldgen", table->boolean("worldgen").value_or(false));
    declare("datapacks", table->boolean("datapacks").value_or(false));
    return capabilities;
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
    const std::optional<std::string_view> main = table->string("main");
    if (!main.has_value() || !main->ends_with(".wasm")) {
        return std::unexpected(ManifestError{std::string{*id} + " needs a 'main' naming its .wasm module"});
    }
    const std::int64_t abi = table->integer("abi").value_or(0);
    if (abi != PluginManifest::kWasmAbi) {
        return std::unexpected(ManifestError{
            std::string{*id} + " is a WebAssembly plugin and needs abi = " + std::to_string(PluginManifest::kWasmAbi) +
            " in plugin.toml" + (abi == 0 ? std::string{} : ", not " + std::to_string(abi))});
    }
    std::expected<PluginCapabilities, ManifestError> capabilities = capabilitiesOf(table->table("capabilities"));
    if (!capabilities.has_value()) {
        return std::unexpected(ManifestError{std::string{*id} + ": " + capabilities.error().message});
    }
    const TomlTable* resources = table->table("resources");
    return PluginManifest{
        .id = std::string{*id},
        .name = std::string{table->string("name").value_or(*id)},
        .version = std::string{*version},
        .authors = table->stringList("authors"),
        .api = static_cast<std::int32_t>(*api),
        .abi = static_cast<std::int32_t>(abi),
        .main = std::string{*main},
        .dependencies = dependenciesOf(*table),
        .capabilities = std::move(*capabilities),
        .exports = exportsOf(table->table("exports")),
        .privateResources = resources == nullptr ? std::vector<std::string>{} : resources->stringList("private"),
    };
}
} // namespace basalt::wire::plugins
