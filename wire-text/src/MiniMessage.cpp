#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::text::miniMessage {
namespace {

constexpr std::string_view kGradientClose = "</gradient>";

std::string toLower(std::string_view value) {
    std::string out{value};
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

std::vector<std::string> splitTagArguments(std::string_view raw) {
    std::vector<std::string> out;
    std::string current;
    char quote = '\0';
    for (const char c : raw) {
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else {
                current += c;
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if (c == ':') {
            out.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    out.push_back(current);
    return out;
}

Component joinSiblings(std::vector<Component> parts) {
    if (parts.empty()) {
        return Component::empty();
    }
    if (parts.size() == 1) return std::move(parts.front());
    // A styled first span is not the parent of subsequent sibling spans.
    return Component::empty().withChildren(std::move(parts));
}

std::string decorationName(std::string name) {
    if (name.starts_with('!')) name.erase(0, 1);
    if (name == "b") return "bold";
    if (name == "i" || name == "em") return "italic";
    if (name == "u") return "underlined";
    if (name == "st") return "strikethrough";
    if (name == "obf") return "obfuscated";
    return name;
}

struct OpenTag {
    std::string name;
    Style style;
};

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    Component parse();

private:
    [[nodiscard]] Style currentStyle() const { return stack_.empty() ? Style{} : stack_.back().style; }

    void pushTag(std::string name, const Style& style) {
        stack_.push_back(OpenTag{std::move(name), currentStyle().merge(style)});
    }

    void popTag(std::string_view name);
    void applyTag(std::string_view rawTag);
    Component parseGradientBlock(std::span<const std::string> colorArgs);

    std::string_view text_;
    std::size_t position_ = 0;
    std::vector<OpenTag> stack_;
};

void Parser::popTag(std::string_view name) {
    if (name.empty()) {
        if (!stack_.empty()) {
            stack_.pop_back();
        }
        return;
    }
    const std::string canonical = decorationName(std::string{name});
    for (std::size_t i = stack_.size(); i > 0; --i) {
        if (stack_[i - 1].name == canonical) {
            stack_.resize(i - 1);
            return;
        }
    }
}

void Parser::applyTag(std::string_view rawTag) {
    const std::vector<std::string> parts = splitTagArguments(rawTag);
    const std::string rawName = toLower(parts.front());
    const bool negated = rawName.starts_with('!');
    const std::string name = decorationName(rawName);

    const auto argument = [&parts](std::size_t index) -> const std::string* {
        return index + 1 < parts.size() ? &parts[index + 1] : nullptr;
    };

    if (name == "reset" && !negated) {
        stack_.clear();
        return;
    }

    Style style;
    bool enabled = !negated;
    if (const std::string* value = argument(0); value != nullptr && toLower(*value) == "false") enabled = false;
    if (name == "bold") {
        style.bold = enabled;
    } else if (name == "italic") {
        style.italic = enabled;
    } else if (name == "underlined") {
        style.underlined = enabled;
    } else if (name == "strikethrough") {
        style.strikethrough = enabled;
    } else if (name == "obfuscated") {
        style.obfuscated = enabled;
    } else if (negated) {
        return;
    } else if (name == "color" || name == "colour" || name == "c") {
        if (const std::string* value = argument(0)) {
            style.color = TextColor::parse(*value);
        }
    } else if (name == "click") {
        const std::string* action = argument(0);
        const std::string* value = argument(1);
        if (action == nullptr || value == nullptr) {
            return;
        }
        const std::optional<ClickEvent::Action> parsed = clickActionFromId(*action);
        if (!parsed) {
            return;
        }
        style.clickEvent = ClickEvent{*parsed, *value};
    } else if (name == "hover") {
        const std::string* action = argument(0);
        const std::string* value = argument(1);
        if (action == nullptr || value == nullptr || *action != "show_text") {
            return;
        }
        style.hoverEvent = HoverEvent::showText(Component::text(*value));
    } else {
        const std::optional<TextColor> color =
            name.starts_with('#') ? TextColor::fromHex(name) : TextColor::named(name);
        if (!color) {
            return;
        }
        style.color = color;
    }

    pushTag(name, style);
}

Component Parser::parseGradientBlock(std::span<const std::string> colorArgs) {
    std::vector<TextColor> stops;
    for (const std::string& arg : colorArgs) {
        if (const std::optional<TextColor> color = TextColor::parse(arg)) {
            stops.push_back(*color);
        }
    }

    const std::size_t closeIndex = text_.find(kGradientClose, position_);
    const std::string_view inner = closeIndex == std::string_view::npos
                                       ? text_.substr(position_)
                                       : text_.substr(position_, closeIndex - position_);
    position_ = closeIndex == std::string_view::npos ? text_.size() : closeIndex + kGradientClose.size();

    const Component base = stops.size() >= 2 ? gradient(inner, stops) : Component::text(std::string{inner});
    return base.withStyle(currentStyle().merge(base.style()));
}

Component Parser::parse() {
    std::vector<Component> parts;
    std::string literal;

    const auto flushLiteral = [this, &parts, &literal]() {
        if (!literal.empty()) {
            parts.push_back(Component{Component::Text{literal}, currentStyle(), {}});
            literal.clear();
        }
    };

    while (position_ < text_.size()) {
        const char c = text_[position_];
        if (c == '\\' && position_ + 1 < text_.size() && text_[position_ + 1] == '<') {
            literal += '<';
            position_ += 2;
            continue;
        }
        if (c != '<') {
            literal += c;
            ++position_;
            continue;
        }

        const std::size_t tagEnd = text_.find('>', position_);
        if (tagEnd == std::string_view::npos) {
            literal += c;
            ++position_;
            continue;
        }

        const std::string_view tagContent = text_.substr(position_ + 1, tagEnd - position_ - 1);
        if (tagContent.starts_with('/')) {
            flushLiteral();
            popTag(toLower(trim(tagContent.substr(1))));
            position_ = tagEnd + 1;
            continue;
        }

        const std::vector<std::string> parsedTag = splitTagArguments(tagContent);
        if (toLower(parsedTag.front()) == "gradient") {
            flushLiteral();
            position_ = tagEnd + 1;
            parts.push_back(parseGradientBlock(std::span{parsedTag}.subspan(1)));
            continue;
        }

        flushLiteral();
        position_ = tagEnd + 1;
        applyTag(tagContent);
    }

    flushLiteral();
    return joinSiblings(std::move(parts));
}

} // namespace

Component gradient(std::string_view text, std::span<const TextColor> stops) {
    if (stops.size() < 2 || text.empty()) {
        return Component::text(std::string{text});
    }

    const auto segments = static_cast<float>(text.size() - 1);
    std::vector<Component> children;
    children.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        const float fraction = segments == 0.0F ? 0.0F : static_cast<float>(index) / segments;
        const float scaled = fraction * static_cast<float>(stops.size() - 1);
        const auto segmentIndex =
            static_cast<std::size_t>(std::clamp(static_cast<int>(scaled), 0, static_cast<int>(stops.size()) - 2));
        const float localFraction = scaled - static_cast<float>(segmentIndex);
        const TextColor color = TextColor::lerp(stops[segmentIndex], stops[segmentIndex + 1], localFraction);
        children.push_back(Component::text(std::string(1, text[index]), color));
    }
    return Component{Component::Text{}, Style{}, std::move(children)};
}

Component parse(std::string_view input) { return Parser{input}.parse(); }

} // namespace basalt::wire::text::miniMessage
