#pragma once

#include <basalt/wire/plugins/PluginPackage.hpp>

#include <expected>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace basalt::wire::plugins {
class PluginFolder {
public:
    using Report = std::function<void(std::string_view)>;

    PluginFolder(const PluginPackage& package, const PluginManifest& manifest, std::string root)
        : package_(&package), manifest_(&manifest), root_(std::move(root)) {}

    void materialize(const Report& report) const;

    [[nodiscard]] std::expected<std::map<std::string, std::string, std::less<>>, PluginLoadError> index() const;

private:
    [[nodiscard]] bool isPrivate(std::string_view relative) const;

    const PluginPackage* package_;
    const PluginManifest* manifest_;
    std::string root_;
};
} // namespace basalt::wire::plugins
