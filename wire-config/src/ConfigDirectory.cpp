#include <basalt/wire/config/ConfigDirectory.hpp>
#include <basalt/wire/config/ConfigUpgrade.hpp>
#include <basalt/wire/io/ConfigFiles.hpp>

#include <string>
#include <utility>

namespace basalt::wire::config {

using wire::common::toml::TomlTable;

std::string ConfigDirectory::pathOf(std::string_view fileName) const { return root_ + "/" + std::string{fileName}; }

std::expected<TomlTable, std::string> ConfigDirectory::load(std::string_view fileName,
                                                            std::string_view defaultContents) const {
    const std::string path = pathOf(fileName);
    auto document = wire::io::configFiles::loadToml(root_, fileName, defaultContents);
    if (!document)
        return std::unexpected(document.error());
    if (document->generated)
        logger_.info([&path] { return path + " not found, generating one with default values."; });
    if (!document->generated) {
        static_cast<void>(appendMissingOptions(rules_, path, fileName, defaultContents, document->values, logger_));
    }
    return std::move(document->values);
}

} // namespace basalt::wire::config
