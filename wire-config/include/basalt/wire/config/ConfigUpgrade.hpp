#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::config {

/// The sections of one file whose options are fixed, so a missing one can be told apart from a user-defined table.
struct UpgradableFile {
    std::string file;
    std::vector<std::string> sections;
};

struct UpgradeRules {
    /// Named in the note appended to an upgraded file.
    std::string version;
    std::vector<UpgradableFile> files;
};

/// Appends the missing options of `defaultContents` as comments and returns the keys added.
[[nodiscard]] std::string appendMissingOptions(const UpgradeRules& rules, std::string_view path,
                                               std::string_view fileName, std::string_view defaultContents,
                                               const wire::common::toml::TomlTable& values,
                                               const wire::common::Logger& logger);

} // namespace basalt::wire::config
