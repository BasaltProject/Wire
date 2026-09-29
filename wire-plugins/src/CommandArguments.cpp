#include <basalt/wire/plugins/CommandArguments.hpp>

#include <charconv>
#include <cstdlib>
#include <system_error>

namespace basalt::wire::plugins {

std::optional<std::vector<CommandValue>> parseCommandWords(const CommandSpec& spec,
                                                           std::span<const std::string> words) {
    std::vector<CommandValue> values;
    std::size_t at = spec.literals.size();
    for (const CommandArgument& argument : spec.arguments) {
        if (at >= words.size()) {
            if (!argument.optional) {
                return std::nullopt;
            }
            values.emplace_back();
            continue;
        }
        const std::string& word = words[at];
        if (argument.type == CommandArgumentType::Text) {
            std::string joined;
            for (std::size_t i = at; i < words.size(); ++i) {
                joined += (i == at ? "" : " ") + words[i];
            }
            values.emplace_back(std::move(joined));
            at = words.size();
            continue;
        }
        ++at;
        if (argument.type == CommandArgumentType::Word) {
            values.emplace_back(word);
        } else if (argument.type == CommandArgumentType::Int) {
            std::int64_t number = 0;
            const auto [end, error] = std::from_chars(word.data(), word.data() + word.size(), number);
            if (error != std::errc{} || end != word.data() + word.size() || (argument.min && number < *argument.min) ||
                (argument.max && number > *argument.max)) {
                return std::nullopt;
            }
            values.emplace_back(number);
        } else if (argument.type == CommandArgumentType::Float) {
            char* end = nullptr;
            const double number = std::strtod(word.c_str(), &end);
            if (end != word.c_str() + word.size()) {
                return std::nullopt;
            }
            values.emplace_back(number);
        } else {
            if (word != "true" && word != "false") {
                return std::nullopt;
            }
            values.emplace_back(word == "true");
        }
    }
    if (at < words.size()) {
        return std::nullopt;
    }
    return values;
}

void pushCommandValue(lua::LuaArgs& lua, const CommandValue& value) {
    if (const auto* flag = std::get_if<bool>(&value)) {
        lua.pushBoolean(*flag);
    } else if (const auto* whole = std::get_if<std::int64_t>(&value)) {
        lua.pushInteger(*whole);
    } else if (const auto* real = std::get_if<double>(&value)) {
        lua.pushNumber(*real);
    } else if (const auto* text = std::get_if<std::string>(&value)) {
        lua.pushString(*text);
    } else {
        lua.pushNil();
    }
}

} // namespace basalt::wire::plugins
