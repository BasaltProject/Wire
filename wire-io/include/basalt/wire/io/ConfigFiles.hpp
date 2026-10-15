#pragma once

#include <basalt/wire/common/toml/Toml.hpp>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace basalt::wire::io::configFiles {

struct Document {
    common::toml::TomlTable values;
    bool generated = false;
};

/// Never overwrites an existing file; reads stop at the byte ceiling, including growing files.
[[nodiscard]] std::expected<Document, std::string> loadToml(std::string_view root, std::string_view fileName,
                                                            std::string_view defaults,
                                                            std::size_t maxBytes = 256 * 1024);

} // namespace basalt::wire::io::configFiles
