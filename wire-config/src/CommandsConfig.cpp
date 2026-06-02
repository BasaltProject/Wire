#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/config/CommandsConfig.hpp>

#include <algorithm>
#include <utility>

namespace basalt::wire::config {
namespace {

using common::placeholder::lowercaseAscii;
using common::toml::TomlTable;

[[nodiscard]] std::vector<std::string> messageLines(const TomlTable& table) {
    std::vector<std::string> lines = table.stringList("message");
    if (!lines.empty()) {
        return lines;
    }
    const std::optional<std::string_view> single = table.string("message");
    if (!single.has_value()) {
        return {};
    }
    std::string_view text = *single;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            lines.emplace_back(text.substr(start));
            break;
        }
        lines.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

} // namespace

const CommandOverride& CommandsConfig::get(std::string_view command) const {
    static const CommandOverride untouched;
    const std::string lowercase = lowercaseAscii(command);
    for (const std::pair<std::string, CommandOverride>& entry : overrides_) {
        if (entry.first == lowercase) {
            return entry.second;
        }
    }
    return untouched;
}

const std::string* CommandsConfig::message(std::string_view key) const {
    for (const std::pair<std::string, std::string>& entry : messages_) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

std::expected<CommandsConfig, std::string> CommandsConfig::parse(const TomlTable& rootTable, std::string_view pathName,
                                                                 const common::Logger& logger) {
    const std::string path{pathName};
    const TomlTable* root = &rootTable;

    std::vector<std::pair<std::string, CommandOverride>> overrides;
    if (const TomlTable* declared = root->table("commands"); declared != nullptr) {
        for (const std::string& name : declared->keys()) {
            const TomlTable* table = declared->table(name);
            if (table == nullptr) {
                continue;
            }
            std::optional<std::string> permission;
            if (const std::optional<std::string_view> written = table->string("permission"); written.has_value()) {
                permission = std::string{*written};
            }
            overrides.emplace_back(lowercaseAscii(name),
                                   CommandOverride{.permission = std::move(permission),
                                                   .enabled = table->boolean("enabled").value_or(true),
                                                   .aliases = table->stringList("aliases")});
        }
    }

    std::vector<CustomCommand> custom;
    if (const TomlTable* declared = root->table("custom"); declared != nullptr) {
        for (const std::string& name : declared->keys()) {
            const TomlTable* table = declared->table(name);
            if (table == nullptr) {
                continue;
            }
            std::vector<std::string> message = messageLines(*table);
            if (message.empty()) {
                // Not fatal: a command with nothing to say is a half-written entry, and stopping the server is worse.
                logger.warn([&name, &path] {
                    return "Custom command '" + name + "' in " + path + " has no message; skipping it.";
                });
                continue;
            }
            const std::string lowercase = lowercaseAscii(name);
            custom.push_back(CustomCommand{
                .name = lowercase,
                .permission = std::string{table->string("permission").value_or("basalt.command." + lowercase)},
                .message = std::move(message),
                .aliases = table->stringList("aliases")});
        }
    }

    std::vector<std::pair<std::string, std::string>> messages;
    if (const TomlTable* declared = root->table("messages"); declared != nullptr) {
        for (const std::string& key : declared->keys()) {
            if (const std::optional<std::string_view> written = declared->string(key); written.has_value()) {
                messages.emplace_back(key, std::string{*written});
            }
        }
    }

    if (!custom.empty()) {
        logger.info([&custom, &path] {
            return "Loaded " + std::to_string(custom.size()) + " custom command(s) from " + path + ".";
        });
    }
    std::vector<std::string> disabled;
    for (const std::pair<std::string, CommandOverride>& entry : overrides) {
        if (!entry.second.enabled) {
            disabled.push_back(entry.first);
        }
    }
    if (!disabled.empty()) {
        std::ranges::sort(disabled);
        logger.info([&disabled, &path] {
            std::string named;
            for (const std::string& name : disabled) {
                if (!named.empty()) {
                    named += ", ";
                }
                named += name;
            }
            return "Disabled by " + path + ": " + named + ".";
        });
    }
    CommandOptions options;
    if (const TomlTable* declared = root->table("options"); declared != nullptr) {
        if (declared->find("log-commands") != nullptr && !declared->boolean("log-commands").has_value()) {
            return std::unexpected(path + ": log-commands must be true or false");
        }
        if (declared->find("tab-complete-min-length") != nullptr && !declared->integer("tab-complete-min-length")) {
            return std::unexpected(path + ": tab-complete-min-length must be a whole number");
        }
        options.logCommands = declared->boolean("log-commands").value_or(options.logCommands);
        const std::int64_t minimum = declared->integer("tab-complete-min-length").value_or(0);
        if (minimum < 0 || minimum > 256) {
            return std::unexpected(path + ": tab-complete-min-length must be between 0 and 256");
        }
        options.tabCompleteMinLength = static_cast<std::int32_t>(minimum);
    }
    return CommandsConfig{std::move(overrides), std::move(custom), std::move(messages), options};
}

} // namespace basalt::wire::config
