#include <basalt/wire/text/Serializers.hpp>

#include <string_view>

namespace basalt::wire::text {
namespace {

// Written as an escape rather than a literal ESC byte: a raw control character in source survives
// neither editors nor copy-paste reliably.
constexpr std::string_view kEscape = "\x1B[";
constexpr std::string_view kReset = "\x1B[0m";

/// The text a component contributes on its own, before its children.
void appendOwnText(std::string& out, const Component& component) {
    if (const auto* text = component.as<Component::Text>()) {
        out += text->content;
    } else if (const auto* translatable = component.as<Component::Translatable>()) {
        out += translatable->fallback ? *translatable->fallback : translatable->key;
    } else if (const auto* keybind = component.as<Component::Keybind>()) {
        out += keybind->key;
    } else if (const auto* score = component.as<Component::Score>()) {
        out += score->name;
    } else {
        out += std::get<Component::Selector>(component.payload()).pattern;
    }
}

void appendFlattened(std::string& out, const Component& component) {
    appendOwnText(out, component);
    for (const Component& child : component.children()) {
        appendFlattened(out, child);
    }
}

void appendAnsiCodes(std::string& out, const Style& style) {
    out += kEscape;
    bool first = true;
    const auto code = [&out, &first](std::string_view value) {
        if (!first) {
            out += ';';
        }
        first = false;
        out += value;
    };

    if (style.color) {
        code("38;2;" + std::to_string(style.color->red()) + ';' + std::to_string(style.color->green()) + ';' +
             std::to_string(style.color->blue()));
    }
    if (style.bold == true) {
        code("1");
    }
    if (style.italic == true) {
        code("3");
    }
    if (style.underlined == true) {
        code("4");
    }
    if (style.obfuscated == true) {
        code("5");
    }
    if (style.strikethrough == true) {
        code("9");
    }
    out += 'm';
}

void appendStyled(std::string& out, const Component& component, const Style& inherited, bool& stylesApplied) {
    const Style effective = inherited.merge(component.style());
    if (!effective.isEmpty()) {
        appendAnsiCodes(out, effective);
        stylesApplied = true;
    }
    appendOwnText(out, component);
    for (const Component& child : component.children()) {
        appendStyled(out, child, effective, stylesApplied);
    }
}

} // namespace

std::string toPlainText(const Component& component) {
    std::string out;
    appendFlattened(out, component);
    return out;
}

std::string toAnsi(const Component& component) {
    std::string out;
    bool stylesApplied = false;
    appendStyled(out, component, Style{}, stylesApplied);
    if (stylesApplied) {
        out += kReset;
    }
    return out;
}

} // namespace basalt::wire::text
