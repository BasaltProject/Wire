#include <basalt/wire/common/placeholder/PlaceholderText.hpp>

#include <cstddef>

namespace basalt::wire::common::placeholder {
namespace {

[[nodiscard]] bool isWhitespace(char character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '\f' ||
           character == '\v';
}

[[nodiscard]] std::optional<std::size_t> closingIndex(std::string_view text, std::size_t start) {
    for (std::size_t index = start + 1; index < text.size(); ++index) {
        const char character = text[index];
        if (isWhitespace(character)) {
            return std::nullopt;
        }
        if (character == '%') {
            return index == start + 1 ? std::nullopt : std::optional<std::size_t>{index};
        }
    }
    return std::nullopt;
}

} // namespace

std::string substitute(std::string_view text, const PlaceholderRegistry& registry, const PlaceholderContext& context,
                       const UnresolvedHandler& onUnresolved) {
    if (text.find('%') == std::string_view::npos) {
        return std::string{text};
    }
    std::string result;
    result.reserve(text.size());
    std::size_t index = 0;
    while (index < text.size()) {
        const char character = text[index];
        if (character != '%') {
            result += character;
            ++index;
            continue;
        }
        if (index + 1 < text.size() && text[index + 1] == '%') {
            result += '%';
            index += 2;
            continue;
        }
        const std::optional<std::size_t> close = closingIndex(text, index);
        if (!close.has_value()) {
            result += character;
            ++index;
            continue;
        }
        const std::string_view name = text.substr(index + 1, *close - index - 1);
        const PlaceholderValue value = registry.resolve(name, context);
        if (value.isText()) {
            result += value.text;
        } else {
            const std::optional<std::string> replacement = onUnresolved ? onUnresolved(name, value) : std::nullopt;
            if (replacement.has_value()) {
                result += *replacement;
            } else {
                result += text.substr(index, *close - index + 1);
            }
        }
        index = *close + 1;
    }
    return result;
}

std::vector<std::string> namesIn(std::string_view text) {
    std::vector<std::string> names;
    std::size_t index = 0;
    while (index < text.size()) {
        if (text[index] != '%') {
            ++index;
            continue;
        }
        if (index + 1 < text.size() && text[index + 1] == '%') {
            index += 2;
            continue;
        }
        const std::optional<std::size_t> close = closingIndex(text, index);
        if (!close.has_value()) {
            ++index;
            continue;
        }
        names.emplace_back(text.substr(index + 1, *close - index - 1));
        index = *close + 1;
    }
    return names;
}

bool globMatches(std::string_view pattern, std::string_view value) {
    const std::string lowerPattern = lowercaseAscii(pattern);
    const std::string text = lowercaseAscii(value);

    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t star = lowerPattern.find('*', start);
        if (star == std::string::npos) {
            parts.emplace_back(std::string_view{lowerPattern}.substr(start));
            break;
        }
        parts.emplace_back(std::string_view{lowerPattern}.substr(start, star - start));
        start = star + 1;
    }
    if (parts.size() == 1) {
        return text == parts[0];
    }

    if (!text.starts_with(parts.front()) || !text.ends_with(parts.back())) {
        return false;
    }
    std::size_t index = parts.front().size();
    for (std::size_t part = 1; part + 1 < parts.size(); ++part) {
        if (parts[part].empty()) {
            continue;
        }
        const std::size_t found = text.find(parts[part], index);
        if (found == std::string::npos) {
            return false;
        }
        index = found + parts[part].size();
    }
    return index + parts.back().size() <= text.size();
}

} // namespace basalt::wire::common::placeholder
