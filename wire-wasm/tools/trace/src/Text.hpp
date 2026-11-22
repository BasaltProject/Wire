#pragma once

#include "World.hpp"

#include <basalt/wire/wasm/Memory.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::wasm::trace {

using ArgList = std::vector<std::pair<std::string, std::string>>;

[[nodiscard]] std::string quote(std::string_view text);
[[nodiscard]] std::string number(double value);
[[nodiscard]] std::string number(float value);

/// Nothing means the packed list is malformed; a zero length is an empty list whatever the pointer.
[[nodiscard]] std::optional<ArgList> readArgs(const GuestMemory& memory, std::int32_t ptr, std::int32_t len);
[[nodiscard]] std::string describe(const std::optional<ArgList>& args);

[[nodiscard]] std::optional<std::string> langTemplate(const World& world, std::string_view key);
[[nodiscard]] std::string substitute(std::string_view text, const ArgList& args);

/// A '@' prefix names a lang key; nothing when that key does not exist.
[[nodiscard]] std::optional<std::string> render(const World& world, std::string_view text, const ArgList& args);

[[nodiscard]] const common::toml::TomlValue* configValue(const World& world, std::string_view key);

} // namespace basalt::wire::wasm::trace
