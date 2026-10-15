#include <basalt/wire/text/TranslationResolver.hpp>

#include <charconv>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace basalt::wire::text {
namespace {

constexpr bool isAsciiLetter(char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

constexpr bool isDigit(char c) noexcept { return c >= '0' && c <= '9'; }

struct Specifier {
    std::size_t start = 0;
    std::size_t end = 0;
    std::int32_t positional = 0;
    char kind = '\0';
};

std::optional<Specifier> nextSpecifier(std::string_view format, std::size_t from) {
    const std::size_t at = format.find('%', from);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }

    Specifier out;
    out.start = at;
    std::size_t cursor = at + 1;

    const std::size_t digitsStart = cursor;
    while (cursor < format.size() && isDigit(format[cursor])) {
        ++cursor;
    }
    if (cursor > digitsStart && cursor < format.size() && format[cursor] == '$') {
        std::from_chars(format.data() + digitsStart, format.data() + cursor, out.positional);
        ++cursor;
    } else {
        cursor = at + 1;
    }

    if (cursor >= format.size()) {
        out.end = format.size();
        out.kind = '\0';
        return out;
    }
    const char kind = format[cursor];
    if (!isAsciiLetter(kind) && kind != '%') {
        return nextSpecifier(format, at + 1);
    }
    out.kind = kind;
    out.end = cursor + 1;
    return out;
}

} // namespace

std::string_view LangTable::getOrDefault(std::string_view key, std::string_view fallback) const {
    const auto found = entries_.find(std::string{key});
    return found != entries_.end() ? std::string_view{found->second} : fallback;
}

std::string resolvePlainText(const Component& component, const LangTable& table) {
    std::string out;
    if (const auto* text = component.as<Component::Text>()) {
        out += text->content;
    } else if (const auto* translatable = component.as<Component::Translatable>()) {
        out += resolveTemplate(*translatable, table);
    } else if (const auto* keybind = component.as<Component::Keybind>()) {
        out += keybind->key;
    } else if (const auto* score = component.as<Component::Score>()) {
        out += score->name;
    } else {
        out += std::get<Component::Selector>(component.payload()).pattern;
    }

    for (const Component& child : component.children()) {
        out += resolvePlainText(child, table);
    }
    return out;
}

Component resolveComponent(const Component& component, const LangTable& table) {
    const auto* translatable = component.as<Component::Translatable>();
    Component resolved = translatable == nullptr
                             ? component.withChildren({})
                             : Component::text(resolveTemplate(*translatable, table)).withStyle(component.style());

    std::vector<Component> children;
    children.reserve(component.children().size());
    for (const Component& child : component.children()) {
        children.push_back(resolveComponent(child, table));
    }
    return resolved.withChildren(std::move(children));
}

std::string resolveTemplate(const Component::Translatable& translatable, const LangTable& table) {
    auto resolved = resolveTemplateStrict(translatable, table);
    if (resolved) {
        return std::move(*resolved);
    }
    const std::string_view fallback =
        translatable.fallback ? std::string_view{*translatable.fallback} : std::string_view{translatable.key};
    return std::string{table.getOrDefault(translatable.key, fallback)};
}

std::expected<std::string, TranslationError> resolveTemplateStrict(const Component::Translatable& translatable,
                                                                   const LangTable& table) {
    const std::string_view fallback =
        translatable.fallback ? std::string_view{*translatable.fallback} : std::string_view{translatable.key};
    const std::string_view format = table.getOrDefault(translatable.key, fallback);

    std::string out;
    std::size_t cursor = 0;
    std::int32_t nextArgument = 0;

    while (const std::optional<Specifier> specifier = nextSpecifier(format, cursor)) {
        out.append(format, cursor, specifier->start - cursor);

        if (specifier->kind == '%' && specifier->positional == 0) {
            out += '%';
        } else if (specifier->kind != 's') {
            return std::unexpected{TranslationError::UnsupportedSpecifier};
        } else {
            const std::int32_t index = specifier->positional != 0 ? specifier->positional - 1 : nextArgument++;
            if (index < 0 || static_cast<std::size_t>(index) >= translatable.args.size()) {
                return std::unexpected{TranslationError::ArgumentOutOfRange};
            }
            out += resolvePlainText(translatable.args[static_cast<std::size_t>(index)], table);
        }
        cursor = specifier->end;
    }

    out.append(format, cursor, format.size() - cursor);
    return out;
}

} // namespace basalt::wire::text
