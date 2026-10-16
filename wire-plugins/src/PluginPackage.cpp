#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/plugins/PluginPackage.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::wire::plugins {
namespace {
[[nodiscard]] std::string_view withoutTrailingSlash(std::string_view text) {
    while (text.ends_with('/')) {
        text.remove_suffix(1);
    }
    return text;
}
} // namespace

std::optional<std::string> packageLayout::moduleFile(std::string_view module) {
    if (module.empty() || module.find('/') != std::string_view::npos || module.find('\\') != std::string_view::npos) {
        return std::nullopt;
    }
    std::string path{kSource};
    path.reserve(path.size() + module.size() + 4);
    for (std::size_t index = 0; index < module.size(); ++index) {
        const char letter = module[index];
        if (letter != '.') {
            path.push_back(letter);
            continue;
        }
        if (index == 0 || index + 1 == module.size() || module[index - 1] == '.') {
            return std::nullopt;
        }
        path.push_back('/');
    }
    return path + ".lua";
}

std::optional<std::string> PluginPackage::readText(std::string_view path) const {
    const std::optional<std::vector<std::uint8_t>> bytes = read(path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    return std::string{bytes->begin(), bytes->end()};
}

FolderPackage::FolderPackage(std::string root) : PluginPackage(root), root_(std::move(root)) {}

std::optional<std::string> FolderPackage::workingFolder() const {
    return root_ + "/" + std::string{withoutTrailingSlash(packageLayout::kResources)};
}

std::optional<std::vector<std::uint8_t>> FolderPackage::read(std::string_view path) const {
    return wire::io::fileIO::readBytes(root_ + "/" + std::string{path});
}

std::vector<std::string> FolderPackage::list(std::string_view prefix) const {
    std::vector<std::string> found;
    walk(std::string{withoutTrailingSlash(prefix)}, found);
    std::sort(found.begin(), found.end());
    return found;
}

void FolderPackage::walk(const std::string& relative, std::vector<std::string>& into) const {
    const std::string absolute = relative.empty() ? root_ : root_ + "/" + relative;
    if (!wire::io::fileIO::isDirectory(absolute)) {
        return;
    }
    for (const std::string& name : wire::io::fileIO::list(absolute)) {
        const std::string child = relative.empty() ? name : relative + "/" + name;
        if (wire::io::fileIO::isDirectory(root_ + "/" + child)) {
            walk(child, into);
        } else {
            into.push_back(child);
        }
    }
}

ArchivePackage::ArchivePackage(std::string path, wire::io::ZipReader zip)
    : PluginPackage(std::move(path)), zip_(std::move(zip)) {}

std::expected<std::unique_ptr<ArchivePackage>, PluginLoadError> ArchivePackage::open(std::string path,
                                                                                     std::vector<std::uint8_t> bytes) {
    std::expected<wire::io::ZipReader, wire::io::ZipError> zip = wire::io::ZipReader::open(std::move(bytes));
    if (!zip.has_value()) {
        return std::unexpected(PluginLoadError{path + " is damaged: it is not a readable package"});
    }
    return std::unique_ptr<ArchivePackage>{new ArchivePackage{std::move(path), std::move(*zip)}};
}

std::optional<std::vector<std::uint8_t>> ArchivePackage::read(std::string_view path) const {
    const std::expected<std::vector<std::uint8_t>, wire::io::ZipError> bytes = zip_.read(path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    return *bytes;
}

std::vector<std::string> ArchivePackage::list(std::string_view prefix) const {
    std::vector<std::string> found;
    for (const std::string& name : zip_.entryNames()) {
        if (name.starts_with(prefix) && !name.ends_with('/')) {
            found.push_back(name);
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

std::expected<std::vector<std::unique_ptr<PluginPackage>>, PluginLoadError>
discoverPackages(const std::string& directory, std::string_view extension) {
    std::vector<std::unique_ptr<PluginPackage>> found;
    if (!wire::io::fileIO::isDirectory(directory)) {
        wire::io::fileIO::createDirectories(directory);
        return found;
    }
    std::vector<std::string> names = wire::io::fileIO::list(directory);
    std::sort(names.begin(), names.end());
    for (const std::string& name : names) {
        const std::string path = directory + "/" + name;
        if (wire::io::fileIO::isDirectory(path)) {
            if (wire::io::fileIO::exists(path + "/" + std::string{packageLayout::kManifest})) {
                found.push_back(std::make_unique<FolderPackage>(path));
            }
            continue;
        }
        if (!name.ends_with(extension)) {
            continue;
        }
        std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(path);
        if (!bytes.has_value()) {
            return std::unexpected(PluginLoadError{path + " could not be read"});
        }
        std::expected<std::unique_ptr<ArchivePackage>, PluginLoadError> archive =
            ArchivePackage::open(path, std::move(*bytes));
        if (!archive.has_value()) {
            return std::unexpected(archive.error());
        }
        found.push_back(std::move(*archive));
    }
    return found;
}
} // namespace basalt::wire::plugins
