#include <basalt/wire/io/ConfigFiles.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <memory>
#include <utility>

namespace basalt::wire::io::configFiles {

std::expected<Document, std::string> loadToml(std::string_view root, std::string_view fileName,
                                              std::string_view defaults, std::size_t maxBytes) {
    const std::string path = std::string{root} + "/" + std::string{fileName};
    if (root.empty() || root.find('\0') != std::string_view::npos || fileName.empty() || fileName == "." ||
        fileName == ".." || fileName.find_first_of("/\\") != std::string_view::npos ||
        fileName.find('\0') != std::string_view::npos || maxBytes == 0 ||
        maxBytes == std::numeric_limits<std::size_t>::max() || defaults.size() > maxBytes)
        return std::unexpected(path + ": invalid configuration file request");

    if (!fileIO::createDirectories(std::string{root}))
        return std::unexpected(path + ": failed to create configuration directory");
    bool generated = false;
    if (!fileIO::exists(path)) {
        if (std::FILE* created = std::fopen(path.c_str(), "wbx")) {
            const bool written = std::fwrite(defaults.data(), 1, defaults.size(), created) == defaults.size();
            const bool closed = std::fclose(created) == 0;
            if (!written || !closed)
                return std::unexpected(path + ": failed to write configuration defaults");
            generated = true;
        } else if (errno != EEXIST) {
            return std::unexpected(path + ": failed to open configuration file");
        }
    }
    std::error_code statusError;
    if (!std::filesystem::is_regular_file(std::filesystem::path{path}, statusError) || statusError)
        return std::unexpected(path + ": configuration must be a regular file");
    using File = std::unique_ptr<std::FILE, int (*)(std::FILE*)>;
    File file{std::fopen(path.c_str(), "rb"), std::fclose};
    if (!file)
        return std::unexpected("Failed to read " + path);
    std::string text(maxBytes + 1, '\0');
    const auto read = std::fread(text.data(), 1, text.size(), file.get());
    if (std::ferror(file.get()))
        return std::unexpected("Failed to read " + path);
    if (read > maxBytes)
        return std::unexpected(path + ": configuration exceeds byte limit");
    text.resize(read);
    auto parsed = common::toml::parse(text);
    if (!parsed)
        return std::unexpected(path +
                               " is not valid TOML: " + std::string{common::toml::tomlErrorMessage(parsed.error())});
    return Document{std::move(*parsed), generated};
}

} // namespace basalt::wire::io::configFiles
