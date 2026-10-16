#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::plugins {
struct ManifestError {
    std::string message;
};

enum class PluginPermission : std::uint8_t { Packets, Network, Commands };

[[nodiscard]] std::string_view permissionName(PluginPermission permission);

[[nodiscard]] std::optional<PluginPermission> permissionOf(std::string_view name);

struct PluginCapabilities {
    bool storage = false;
    bool playersData = false;
    bool commands = false;
    std::vector<std::string> files;
    std::vector<std::string> network;
    bool packets = false;

    [[nodiscard]] std::set<PluginPermission> asks() const;
};

struct PluginExports {
    std::vector<std::string> modules;
    std::vector<std::string> services;

    [[nodiscard]] bool opensEverything() const;

    [[nodiscard]] bool exportsModule(std::string_view module) const;
};

struct PluginDependency {
    std::string id;
    std::string requirement;
    bool hard = true;

    [[nodiscard]] bool accepts(std::string_view version) const;
};

[[nodiscard]] std::int32_t compareVersions(std::string_view left, std::string_view right);

struct PluginManifest {
    static constexpr std::int32_t kApi = 1;
    static constexpr std::string_view kFileName = "plugin.toml";

    std::string id;
    std::string name;
    std::string version;
    std::vector<std::string> authors;
    std::int32_t api = 0;
    std::string main;
    std::vector<PluginDependency> dependencies;
    PluginCapabilities capabilities;
    PluginExports exports;
    std::vector<std::string> privateResources;

    [[nodiscard]] std::vector<PluginDependency> hardDependencies() const;

    [[nodiscard]] static std::expected<PluginManifest, ManifestError> parse(std::string_view text);
};
} // namespace basalt::wire::plugins
