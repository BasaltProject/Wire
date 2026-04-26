#include <basalt/wire/plugins/CommandSpec.hpp>

#include <charconv>
#include <cstdint>

namespace basalt::wire::plugins {
namespace {
[[nodiscard]] std::string_view trimmed(std::string_view text) {
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] std::optional<std::int64_t> longOrNull(std::string_view text) {
    std::int64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::vector<std::string> tokenize(std::string_view spec) {
    std::vector<std::string> tokens;
    std::string current;
    char closing = '\0';
    for (const char character : trimmed(spec)) {
        if (closing != '\0') {
            current += character;
            if (character == closing) {
                tokens.push_back(current);
                current.clear();
                closing = '\0';
            }
            continue;
        }
        if (character == '<' || character == '[') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            current += character;
            closing = character == '<' ? '>' : ']';
            continue;
        }
        if (character == ' ') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current += character;
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

[[nodiscard]] std::expected<CommandArgument, ManifestError> argumentOf(std::string_view token, std::string_view spec) {
    const bool optional = token.starts_with('[');
    const char closing = optional ? ']' : '>';
    if (!token.ends_with(closing)) {
        return std::unexpected(ManifestError{"'" + std::string{spec} + "' has an argument that never closes"});
    }
    const std::string_view body = token.substr(1, token.size() - 2);
    const std::size_t colon = body.find(':');
    const std::string_view name = trimmed(colon == std::string_view::npos ? body : body.substr(0, colon));
    if (name.empty()) {
        return std::unexpected(ManifestError{"'" + std::string{spec} + "' has an argument with no name"});
    }
    const std::string_view rest =
        trimmed(colon == std::string_view::npos ? std::string_view{"word"} : body.substr(colon + 1));
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (std::size_t index = 0; index <= rest.size(); ++index) {
        if (index == rest.size() || rest[index] == ' ') {
            if (index > start) {
                parts.push_back(rest.substr(start, index - start));
            }
            start = index + 1;
        }
    }
    const std::string_view typeName = parts.empty() ? std::string_view{"word"} : parts.front();
    const std::optional<CommandArgumentType> type = argumentTypeOf(typeName);
    if (!type.has_value()) {
        return std::unexpected(ManifestError{"'" + std::string{spec} + "' asks for the argument type '" +
                                             std::string{typeName} + "', which this server does not have yet"});
    }
    CommandArgument argument{
        .name = std::string{name}, .type = *type, .optional = optional, .min = std::nullopt, .max = std::nullopt};
    if (parts.size() >= 2) {
        const std::string_view range = parts[1];
        if (const std::size_t dots = range.find(".."); dots != std::string_view::npos) {
            argument.min = longOrNull(range.substr(0, dots));
            argument.max = longOrNull(range.substr(dots + 2));
        }
    }
    return argument;
}
} // namespace

std::string_view spellingOf(CommandArgumentType type) {
    switch (type) {
    case CommandArgumentType::Word:
        return "word";
    case CommandArgumentType::Text:
        return "text";
    case CommandArgumentType::Int:
        return "int";
    case CommandArgumentType::Float:
        return "float";
    case CommandArgumentType::Bool:
    default:
        return "bool";
    }
}

std::optional<CommandArgumentType> argumentTypeOf(std::string_view spelling) {
    if (spelling == "word") {
        return CommandArgumentType::Word;
    }
    if (spelling == "text") {
        return CommandArgumentType::Text;
    }
    if (spelling == "int") {
        return CommandArgumentType::Int;
    }
    if (spelling == "float") {
        return CommandArgumentType::Float;
    }
    if (spelling == "bool") {
        return CommandArgumentType::Bool;
    }
    return std::nullopt;
}

std::string CommandSpec::usage() const {
    std::string text;
    for (const std::string& literal : literals) {
        if (!text.empty()) {
            text += ' ';
        }
        text += literal;
    }
    for (const CommandArgument& argument : arguments) {
        if (!text.empty()) {
            text += ' ';
        }
        std::string range;
        if (argument.min.has_value() || argument.max.has_value()) {
            range = " " + (argument.min.has_value() ? std::to_string(*argument.min) : "null") + ".." +
                    (argument.max.has_value() ? std::to_string(*argument.max) : "null");
        }
        const std::string body = argument.name + ": " + std::string{spellingOf(argument.type)} + range;
        text += argument.optional ? "[" + body + "]" : "<" + body + ">";
    }
    return text;
}

std::expected<CommandSpec, ManifestError> CommandSpec::parse(std::string_view spec,
                                                             std::optional<std::string> permission) {
    const std::vector<std::string> tokens = tokenize(spec);
    if (tokens.empty()) {
        return std::unexpected(ManifestError{"a command with no name"});
    }
    CommandSpec built;
    built.permission = std::move(permission);
    for (const std::string& token : tokens) {
        if (token.starts_with('<') || token.starts_with('[')) {
            std::expected<CommandArgument, ManifestError> argument = argumentOf(token, spec);
            if (!argument.has_value()) {
                return std::unexpected(argument.error());
            }
            built.arguments.push_back(std::move(*argument));
            continue;
        }
        if (!built.arguments.empty()) {
            return std::unexpected(
                ManifestError{"'" + std::string{spec} + "' puts the word '" + token + "' after an argument"});
        }
        built.literals.push_back(token);
    }
    if (built.literals.empty()) {
        return std::unexpected(ManifestError{"'" + std::string{spec} + "' starts with an argument instead of a name"});
    }
    bool seenOptional = false;
    for (const CommandArgument& argument : built.arguments) {
        if (seenOptional && !argument.optional) {
            return std::unexpected(ManifestError{"'" + std::string{spec} + "' has the required '" + argument.name +
                                                 "' after an optional one"});
        }
        seenOptional = seenOptional || argument.optional;
    }
    return built;
}
} // namespace basalt::wire::plugins
