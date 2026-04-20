#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/plugins/PluginFolder.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace basalt::wire::plugins {
namespace {
[[nodiscard]] bool isAddressedByName(std::string_view path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos) {
        return false;
    }
    const std::string_view extension = path.substr(dot + 1);
    return extension == "toml" || extension == "json";
}

[[nodiscard]] std::string_view fileName(std::string_view path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

void walk(const std::string& directory, std::vector<std::string>& into) {
    if (!wire::io::fileIO::isDirectory(directory)) {
        return;
    }
    std::vector<std::string> names = wire::io::fileIO::list(directory);
    std::sort(names.begin(), names.end());
    for (const std::string& name : names) {
        const std::string path = directory + "/" + name;
        if (wire::io::fileIO::isDirectory(path)) {
            walk(path, into);
        } else {
            into.push_back(path);
        }
    }
}
} // namespace

bool PluginFolder::isPrivate(std::string_view relative) const {
    for (const std::string& pattern : manifest_->privateResources) {
        std::string_view clean{pattern};
        if (clean.starts_with('/')) {
            clean.remove_prefix(1);
        }
        if (clean.ends_with('/') ? relative.starts_with(clean) : relative == clean) {
            return true;
        }
    }
    return false;
}

void PluginFolder::materialize(const Report& report) const {
    wire::io::fileIO::createDirectories(root_);
    if (package_->workingFolder().has_value()) {
        return;
    }
    for (const std::string& path : package_->list(packageLayout::kResources)) {
        const std::string_view relative = std::string_view{path}.substr(packageLayout::kResources.size());
        if (isPrivate(relative)) {
            continue;
        }
        const std::string live = root_ + "/" + std::string{relative};
        if (wire::io::fileIO::exists(live)) {
            continue;
        }
        const std::optional<std::vector<std::uint8_t>> bytes = package_->read(path);
        if (!bytes.has_value()) {
            continue;
        }
        wire::io::fileIO::createDirectories(live.substr(0, live.find_last_of('/')));
        wire::io::fileIO::writeBytes(live, *bytes);
        if (report) {
            report("wrote " + live);
        }
    }
}

std::expected<std::map<std::string, std::string, std::less<>>, PluginLoadError> PluginFolder::index() const {
    std::vector<std::string> byPath;
    walk(root_, byPath);

    std::map<std::string, std::vector<std::string>, std::less<>> byName;
    for (const std::string& path : byPath) {
        if (isAddressedByName(path)) {
            byName[std::string{fileName(path)}].push_back(path);
        }
    }
    for (const auto& [name, paths] : byName) {
        if (paths.size() <= 1) {
            continue;
        }
        std::string joined;
        for (const std::string& path : paths) {
            if (!joined.empty()) {
                joined += " and ";
            }
            joined += path;
        }
        return std::unexpected(
            PluginLoadError{"two files are called '" + name + "' (" + joined + "), and one name has to mean one file"});
    }

    std::map<std::string, std::string, std::less<>> index;
    const std::string prefix = root_ + "/";
    for (const std::string& path : byPath) {
        index.emplace(path.starts_with(prefix) ? path.substr(prefix.size()) : path, path);
    }
    return index;
}
} // namespace basalt::wire::plugins
