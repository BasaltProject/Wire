#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::config {

struct CommandOverride {
    std::optional<std::string> permission;
    bool enabled = true;
    std::vector<std::string> aliases;

    friend bool operator==(const CommandOverride&, const CommandOverride&) = default;
};

struct CustomCommand {
    std::string name;
    std::string permission;
    std::vector<std::string> message;
    std::vector<std::string> aliases;

    friend bool operator==(const CustomCommand&, const CustomCommand&) = default;
};

struct CommandOptions {
    bool logCommands = true;
    std::int32_t tabCompleteMinLength = 0;
};

/// The commands.toml shared by every Basalt product: renames, permissions, removals and `[custom]` commands.
class CommandsConfig {
public:
    static constexpr std::string_view kFileName = "commands.toml";

    CommandsConfig() = default;

    CommandsConfig(std::vector<std::pair<std::string, CommandOverride>> overrides, std::vector<CustomCommand> custom,
                   std::vector<std::pair<std::string, std::string>> messages, CommandOptions options = {})
        : overrides_(std::move(overrides)), custom_(std::move(custom)), messages_(std::move(messages)),
          options_(options) {}

    [[nodiscard]] static std::expected<CommandsConfig, std::string>
    parse(const common::toml::TomlTable& root, std::string_view path,
          const common::Logger& logger = common::Logger{"Config"});

    [[nodiscard]] const CommandOptions& options() const noexcept { return options_; }

    [[nodiscard]] const CommandOverride& get(std::string_view command) const;

    [[nodiscard]] const std::vector<std::pair<std::string, CommandOverride>>& overrides() const noexcept {
        return overrides_;
    }

    [[nodiscard]] const std::vector<CustomCommand>& custom() const noexcept { return custom_; }

    [[nodiscard]] const std::string* message(std::string_view key) const;

    [[nodiscard]] const std::vector<std::pair<std::string, std::string>>& messages() const noexcept {
        return messages_;
    }

private:
    std::vector<std::pair<std::string, CommandOverride>> overrides_;
    std::vector<CustomCommand> custom_;
    std::vector<std::pair<std::string, std::string>> messages_;
    CommandOptions options_;
};

} // namespace basalt::wire::config
