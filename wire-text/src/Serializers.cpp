#include <basalt/wire/text/Serializers.hpp>

#include <array>
#include <cstdint>
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

/// The sixteen colours a `§` code can name, in the order of their codes.
constexpr std::array<TextColor, 16> kLegacyColors{kBlack, kDarkBlue,    kDarkGreen, kDarkAqua, kDarkRed, kDarkPurple,
                                                  kGold,  kGray,        kDarkGray,  kBlue,     kGreen,   kAqua,
                                                  kRed,   kLightPurple, kYellow,    kWhite};

constexpr std::string_view kLegacyCodes = "0123456789abcdef";

/// The code of the nearest of the sixteen, by plain distance in RGB. A colour the other side cannot
/// draw is drawn as the one it can that is closest to it.
[[nodiscard]] char legacyCodeFor(const TextColor color) {
    std::size_t best = 0;
    std::int64_t bestDistance = -1;
    for (std::size_t index = 0; index < kLegacyColors.size(); ++index) {
        const std::int64_t red = color.red() - kLegacyColors[index].red();
        const std::int64_t green = color.green() - kLegacyColors[index].green();
        const std::int64_t blue = color.blue() - kLegacyColors[index].blue();
        const std::int64_t distance = (red * red) + (green * green) + (blue * blue);
        if (bestDistance < 0 || distance < bestDistance) {
            bestDistance = distance;
            best = index;
        }
    }
    return kLegacyCodes[best];
}

/// The codes one style needs, in the order a client reads them: the colour resets everything, so it
/// goes first and the decorations follow it.
void appendLegacyCodes(std::string& out, const Style& style) {
    const auto code = [&out](const char letter) {
        out += "\u00a7";
        out += letter;
    };
    if (style.color) {
        code(legacyCodeFor(*style.color));
    }
    if (style.obfuscated == true) {
        code('k');
    }
    if (style.bold == true) {
        code('l');
    }
    if (style.strikethrough == true) {
        code('m');
    }
    if (style.underlined == true) {
        code('n');
    }
    if (style.italic == true) {
        code('o');
    }
}

void appendLegacy(std::string& out, const Component& component, const Style& inherited) {
    const Style effective = inherited.merge(component.style());
    if (!effective.isEmpty()) {
        appendLegacyCodes(out, effective);
    }
    appendOwnText(out, component);
    for (const Component& child : component.children()) {
        appendLegacy(out, child, effective);
    }
}

} // namespace

std::string toPlainText(const Component& component) {
    std::string out;
    appendFlattened(out, component);
    return out;
}

std::string toLegacyText(const Component& component) {
    std::string out;
    appendLegacy(out, component, Style{});
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
