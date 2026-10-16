#pragma once

#include <basalt/wire/plugins/PluginManifest.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::plugins {
enum class CommandArgumentType : std::uint8_t { Word, Text, Int, Float, Bool };

[[nodiscard]] std::string_view spellingOf(CommandArgumentType type);

[[nodiscard]] std::optional<CommandArgumentType> argumentTypeOf(std::string_view spelling);

struct CommandArgument {
    std::string name;
    CommandArgumentType type = CommandArgumentType::Word;
    bool optional = false;
    std::optional<std::int64_t> min;
    std::optional<std::int64_t> max;
};

struct CommandSpec {
    std::vector<std::string> literals;
    std::vector<CommandArgument> arguments;
    std::optional<std::string> permission;

    [[nodiscard]] const std::string& name() const { return literals.front(); }

    [[nodiscard]] std::string usage() const;

    [[nodiscard]] static std::expected<CommandSpec, ManifestError>
    parse(std::string_view spec, std::optional<std::string> permission = std::nullopt);
};
} // namespace basalt::wire::plugins
