#include <basalt/wire/text/Component.hpp>

namespace basalt::wire::text {
namespace {

/// Two optional sub-components are equal when both are absent, or both present and equal. Pointer
/// identity is not the question: components are values.
bool sameComponent(const std::shared_ptr<const Component>& lhs, const std::shared_ptr<const Component>& rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }
    return *lhs == *rhs;
}

} // namespace

bool operator==(const Component::Translatable& lhs, const Component::Translatable& rhs) {
    return lhs.key == rhs.key && lhs.fallback == rhs.fallback && lhs.args == rhs.args;
}

bool operator==(const Component::Selector& lhs, const Component::Selector& rhs) {
    return lhs.pattern == rhs.pattern && sameComponent(lhs.separator, rhs.separator);
}

bool operator==(const Component& lhs, const Component& rhs) {
    return lhs.payload_ == rhs.payload_ && lhs.style_ == rhs.style_ && lhs.children_ == rhs.children_;
}

Component::Component() : payload_(Text{}) {}

Component::Component(Payload payload) : payload_(std::move(payload)) {}

Component::Component(Payload payload, Style style, std::vector<Component> children)
    : payload_(std::move(payload)), style_(std::move(style)), children_(std::move(children)) {}

Component::Component(const Component&) = default;

Component::Component(Component&&) noexcept = default;

Component& Component::operator=(const Component&) = default;

Component& Component::operator=(Component&&) noexcept = default;

Component::~Component() = default;

Component Component::text(std::string content) { return Component{Text{std::move(content)}}; }

Component Component::text(std::string content, TextColor color) {
    Style style;
    style.color = color;
    return Component{Text{std::move(content)}, std::move(style), {}};
}

Component Component::translatable(std::string key, std::vector<Component> args) {
    return Component{Translatable{std::move(key), std::move(args), std::nullopt}};
}

Component Component::keybind(std::string key) { return Component{Keybind{std::move(key)}}; }

Component Component::score(std::string name, std::string objective) {
    return Component{Score{std::move(name), std::move(objective)}};
}

Component Component::empty() { return Component{Text{}}; }

Component Component::join(const Component& separator, const std::vector<Component>& components) {
    if (components.empty()) {
        return empty();
    }
    std::vector<Component> merged;
    merged.reserve(components.size() * 2 - 1);
    for (std::size_t i = 0; i < components.size(); ++i) {
        if (i > 0) {
            merged.push_back(separator);
        }
        merged.push_back(components[i]);
    }
    return Component{Text{}, Style{}, std::move(merged)};
}

Component Component::withStyle(Style newStyle) const { return Component{payload_, std::move(newStyle), children_}; }

Component Component::withChildren(std::vector<Component> newChildren) const {
    return Component{payload_, style_, std::move(newChildren)};
}

Component Component::append(Component child) const {
    std::vector<Component> next = children_;
    next.push_back(std::move(child));
    return Component{payload_, style_, std::move(next)};
}

Component Component::color(std::optional<TextColor> value) const {
    Style next = style_;
    next.color = value;
    return withStyle(std::move(next));
}

Component Component::bold(bool value) const {
    Style next = style_;
    next.bold = value;
    return withStyle(std::move(next));
}

Component Component::italic(bool value) const {
    Style next = style_;
    next.italic = value;
    return withStyle(std::move(next));
}

Component Component::underlined(bool value) const {
    Style next = style_;
    next.underlined = value;
    return withStyle(std::move(next));
}

Component Component::strikethrough(bool value) const {
    Style next = style_;
    next.strikethrough = value;
    return withStyle(std::move(next));
}

Component Component::obfuscated(bool value) const {
    Style next = style_;
    next.obfuscated = value;
    return withStyle(std::move(next));
}

Component Component::clickEvent(std::optional<ClickEvent> event) const {
    Style next = style_;
    next.clickEvent = std::move(event);
    return withStyle(std::move(next));
}

Component Component::hoverEvent(std::optional<HoverEvent> event) const {
    Style next = style_;
    next.hoverEvent = std::move(event);
    return withStyle(std::move(next));
}

Style Style::merge(const Style& other) const {
    Style result;
    result.color = other.color ? other.color : color;
    result.bold = other.bold ? other.bold : bold;
    result.italic = other.italic ? other.italic : italic;
    result.underlined = other.underlined ? other.underlined : underlined;
    result.strikethrough = other.strikethrough ? other.strikethrough : strikethrough;
    result.obfuscated = other.obfuscated ? other.obfuscated : obfuscated;
    result.font = other.font ? other.font : font;
    result.insertion = other.insertion ? other.insertion : insertion;
    result.clickEvent = other.clickEvent ? other.clickEvent : clickEvent;
    result.hoverEvent = other.hoverEvent ? other.hoverEvent : hoverEvent;
    return result;
}

bool operator==(const HoverEvent::ShowText& lhs, const HoverEvent::ShowText& rhs) {
    return sameComponent(lhs.value, rhs.value);
}

bool operator==(const HoverEvent::ShowEntity& lhs, const HoverEvent::ShowEntity& rhs) {
    return lhs.entityType == rhs.entityType && lhs.uuid == rhs.uuid && sameComponent(lhs.name, rhs.name);
}

std::string_view HoverEvent::actionId() const noexcept {
    switch (payload.index()) {
    case 0:
        return "show_text";
    case 1:
        return "show_item";
    default:
        return "show_entity";
    }
}

HoverEvent HoverEvent::showText(Component value) {
    return HoverEvent{ShowText{std::make_shared<const Component>(std::move(value))}};
}

ClickEvent ClickEvent::changePage(std::int32_t page) { return {Action::ChangePage, std::to_string(page)}; }

std::string_view clickActionId(ClickEvent::Action action) noexcept {
    switch (action) {
    case ClickEvent::Action::OpenUrl:
        return "open_url";
    case ClickEvent::Action::RunCommand:
        return "run_command";
    case ClickEvent::Action::SuggestCommand:
        return "suggest_command";
    case ClickEvent::Action::ChangePage:
        return "change_page";
    case ClickEvent::Action::CopyToClipboard:
        return "copy_to_clipboard";
    }
    return "run_command";
}

std::optional<ClickEvent::Action> clickActionFromId(std::string_view id) noexcept {
    if (id == "open_url") {
        return ClickEvent::Action::OpenUrl;
    }
    if (id == "run_command") {
        return ClickEvent::Action::RunCommand;
    }
    if (id == "suggest_command") {
        return ClickEvent::Action::SuggestCommand;
    }
    if (id == "change_page") {
        return ClickEvent::Action::ChangePage;
    }
    if (id == "copy_to_clipboard") {
        return ClickEvent::Action::CopyToClipboard;
    }
    return std::nullopt;
}

} // namespace basalt::wire::text
