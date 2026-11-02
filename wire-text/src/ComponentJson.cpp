#include <basalt/wire/text/ComponentJson.hpp>

#include <cstdint>
#include <cstdlib>

namespace basalt::wire::text::componentJson {
common::JsonValue serializeWith(const Component& component, bool legacy);

namespace {

using common::JsonObject;
using common::JsonValue;

JsonValue serializeAll(const std::vector<Component>& components, bool legacy) {
    std::vector<JsonValue> out;
    out.reserve(components.size());
    for (const Component& component : components) {
        out.push_back(serializeWith(component, legacy));
    }
    return JsonValue::ofArray(std::move(out));
}

void writeStyle(JsonObject& object, const Style& style, bool legacy) {
    if (style.color) {
        const std::optional<std::string_view> name = TextColor::nameOf(*style.color);
        object.set("color", JsonValue::of(name ? std::string{*name} : style.color->toHexString()));
    }
    if (style.bold) {
        object.set("bold", JsonValue::of(*style.bold));
    }
    if (style.italic) {
        object.set("italic", JsonValue::of(*style.italic));
    }
    if (style.underlined) {
        object.set("underlined", JsonValue::of(*style.underlined));
    }
    if (style.strikethrough) {
        object.set("strikethrough", JsonValue::of(*style.strikethrough));
    }
    if (style.obfuscated) {
        object.set("obfuscated", JsonValue::of(*style.obfuscated));
    }
    if (style.font) {
        object.set("font", JsonValue::of(*style.font));
    }
    if (style.insertion) {
        object.set("insertion", JsonValue::of(*style.insertion));
    }
    if (legacy) {
        if (style.clickEvent) {
            JsonObject click;
            click.set("action", JsonValue::of(std::string{clickActionId(style.clickEvent->action)}));
            click.set("value", JsonValue::of(style.clickEvent->value));
            object.set("clickEvent", JsonValue::ofObject(std::move(click)));
        }
        if (style.hoverEvent) {
            JsonObject hover;
            hover.set("action", JsonValue::of(std::string{style.hoverEvent->actionId()}));
            if (const auto* showText = std::get_if<HoverEvent::ShowText>(&style.hoverEvent->payload)) {
                hover.set("contents", showText->value ? serializeWith(*showText->value, true) : JsonValue{});
            } else if (const auto* showItem = std::get_if<HoverEvent::ShowItem>(&style.hoverEvent->payload)) {
                JsonObject contents;
                contents.set("id", JsonValue::of(showItem->itemId));
                contents.set("count", JsonValue::of(static_cast<std::int64_t>(showItem->count)));
                hover.set("contents", JsonValue::ofObject(std::move(contents)));
            } else {
                const auto& showEntity = std::get<HoverEvent::ShowEntity>(style.hoverEvent->payload);
                JsonObject contents;
                contents.set("type", JsonValue::of(showEntity.entityType));
                contents.set("id", JsonValue::of(showEntity.uuid));
                if (showEntity.name) {
                    contents.set("name", serializeWith(*showEntity.name, true));
                }
                hover.set("contents", JsonValue::ofObject(std::move(contents)));
            }
            object.set("hoverEvent", JsonValue::ofObject(std::move(hover)));
        }
        return;
    }
    if (style.clickEvent) {
        const ClickEvent& event = *style.clickEvent;
        JsonObject click;
        click.set("action", JsonValue::of(std::string{clickActionId(event.action)}));
        switch (event.action) {
        case ClickEvent::Action::OpenUrl:
            click.set("url", JsonValue::of(event.value));
            break;
        case ClickEvent::Action::RunCommand:
        case ClickEvent::Action::SuggestCommand:
            click.set("command", JsonValue::of(event.value));
            break;
        case ClickEvent::Action::ChangePage:
            click.set("page", JsonValue::of(static_cast<std::int64_t>(std::atoi(event.value.c_str()))));
            break;
        case ClickEvent::Action::CopyToClipboard:
            click.set("value", JsonValue::of(event.value));
            break;
        }
        object.set("click_event", JsonValue::ofObject(std::move(click)));
    }
    if (style.hoverEvent) {
        JsonObject hover;
        hover.set("action", JsonValue::of(std::string{style.hoverEvent->actionId()}));
        if (const auto* showText = std::get_if<HoverEvent::ShowText>(&style.hoverEvent->payload)) {
            hover.set("value", showText->value ? serializeWith(*showText->value, legacy) : JsonValue{});
        } else if (const auto* showItem = std::get_if<HoverEvent::ShowItem>(&style.hoverEvent->payload)) {
            hover.set("id", JsonValue::of(showItem->itemId));
            hover.set("count", JsonValue::of(static_cast<std::int64_t>(showItem->count)));
        } else {
            const auto& showEntity = std::get<HoverEvent::ShowEntity>(style.hoverEvent->payload);
            hover.set("id", JsonValue::of(showEntity.entityType));
            hover.set("uuid", JsonValue::of(showEntity.uuid));
            if (showEntity.name) {
                hover.set("name", serializeWith(*showEntity.name, legacy));
            }
        }
        object.set("hover_event", JsonValue::ofObject(std::move(hover)));
    }
}

std::optional<std::string> readString(const JsonObject& object, std::string_view key) {
    const JsonValue* value = object.find(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    const auto* text = value->get<std::string>();
    return text != nullptr ? std::optional<std::string>{*text} : std::nullopt;
}

std::optional<bool> readBoolean(const JsonObject& object, std::string_view key) {
    const JsonValue* value = object.find(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    const auto* boolean = value->get<bool>();
    return boolean != nullptr ? std::optional<bool>{*boolean} : std::nullopt;
}

std::int32_t readInt(const JsonObject& object, std::string_view key, std::int32_t fallback) {
    const JsonValue* value = object.find(key);
    if (value == nullptr) {
        return fallback;
    }
    if (const auto* integer = value->get<std::int64_t>()) {
        return static_cast<std::int32_t>(*integer);
    }
    if (const auto* number = value->get<double>()) {
        return static_cast<std::int32_t>(*number);
    }
    return fallback;
}

const JsonObject* readObject(const JsonObject& object, std::string_view key) {
    const JsonValue* value = object.find(key);
    return value != nullptr ? value->get<JsonObject>() : nullptr;
}

std::vector<Component> deserializeAll(const JsonObject& object, std::string_view key) {
    std::vector<Component> out;
    const JsonValue* value = object.find(key);
    if (value == nullptr) {
        return out;
    }
    const auto* array = value->get<std::vector<JsonValue>>();
    if (array == nullptr) {
        return out;
    }
    out.reserve(array->size());
    for (const JsonValue& element : *array) {
        out.push_back(deserialize(element));
    }
    return out;
}

std::optional<ClickEvent> readClickEvent(const JsonObject& object) {
    const std::optional<std::string> action = readString(object, "action");
    if (!action) {
        return std::nullopt;
    }
    const std::optional<ClickEvent::Action> parsed = clickActionFromId(*action);
    if (!parsed) {
        return std::nullopt;
    }
    if (*parsed == ClickEvent::Action::ChangePage) {
        if (const JsonValue* page = object.find("page")) {
            if (const auto* integer = page->get<std::int64_t>()) {
                return ClickEvent::changePage(static_cast<std::int32_t>(*integer));
            }
        }
    }
    for (const std::string_view key : {"url", "command", "value"}) {
        if (const std::optional<std::string> value = readString(object, key)) {
            return ClickEvent{*parsed, *value};
        }
    }
    return std::nullopt;
}

std::optional<HoverEvent> readHoverEvent(const JsonObject& object) {
    const std::optional<std::string> action = readString(object, "action");
    if (!action) {
        return std::nullopt;
    }
    const JsonValue* contents = object.find("contents");
    if (*action == "show_text") {
        const JsonValue* value = object.find("value");
        if (value == nullptr) {
            value = contents;
        }
        return value != nullptr ? std::optional<HoverEvent>{HoverEvent::showText(deserialize(*value))} : std::nullopt;
    }
    const JsonObject* body =
        contents != nullptr && contents->get<JsonObject>() != nullptr ? contents->get<JsonObject>() : &object;
    if (*action == "show_item") {
        return HoverEvent::showItem(readString(*body, "id").value_or(""), readInt(*body, "count", 1));
    }
    if (*action == "show_entity") {
        const bool legacy = body != &object;
        HoverEvent::ShowEntity entity{readString(*body, legacy ? "type" : "id").value_or(""),
                                      readString(*body, legacy ? "id" : "uuid").value_or(""), nullptr};
        if (const JsonValue* name = body->find("name")) {
            entity.name = std::make_shared<const Component>(deserialize(*name));
        }
        return HoverEvent{std::move(entity)};
    }
    return std::nullopt;
}

Style readStyle(const JsonObject& object) {
    Style style;
    if (const std::optional<std::string> color = readString(object, "color")) {
        style.color = TextColor::parse(*color);
    }
    style.bold = readBoolean(object, "bold");
    style.italic = readBoolean(object, "italic");
    style.underlined = readBoolean(object, "underlined");
    style.strikethrough = readBoolean(object, "strikethrough");
    style.obfuscated = readBoolean(object, "obfuscated");
    style.font = readString(object, "font");
    style.insertion = readString(object, "insertion");
    for (const std::string_view key : {"click_event", "clickEvent"}) {
        if (const JsonObject* click = readObject(object, key)) {
            style.clickEvent = readClickEvent(*click);
            break;
        }
    }
    for (const std::string_view key : {"hover_event", "hoverEvent"}) {
        if (const JsonObject* hover = readObject(object, key)) {
            style.hoverEvent = readHoverEvent(*hover);
            break;
        }
    }
    return style;
}

Component deserializeObject(const JsonObject& object) {
    Component::Payload payload = Component::Text{};
    if (const std::optional<std::string> text = readString(object, "text")) {
        payload = Component::Text{*text};
    } else if (const std::optional<std::string> key = readString(object, "translate")) {
        payload = Component::Translatable{*key, deserializeAll(object, "with"), readString(object, "fallback")};
    } else if (const std::optional<std::string> keybind = readString(object, "keybind")) {
        payload = Component::Keybind{*keybind};
    } else if (const JsonObject* score = readObject(object, "score")) {
        payload =
            Component::Score{readString(*score, "name").value_or(""), readString(*score, "objective").value_or("")};
    } else if (const std::optional<std::string> selector = readString(object, "selector")) {
        Component::Selector value{*selector, nullptr};
        if (const JsonValue* separator = object.find("separator")) {
            value.separator = std::make_shared<const Component>(deserialize(*separator));
        }
        payload = std::move(value);
    }

    return Component{std::move(payload), readStyle(object), deserializeAll(object, "extra")};
}

Component deserializeSiblingChain(const std::vector<JsonValue>& array) {
    if (array.empty()) {
        return Component::empty();
    }
    Component result = deserialize(array.front());
    for (std::size_t i = 1; i < array.size(); ++i) {
        result = result.append(deserialize(array[i]));
    }
    return result;
}

} // namespace

JsonValue serializeWith(const Component& component, bool legacy) {
    JsonObject object;
    if (const auto* text = component.as<Component::Text>()) {
        object.set("text", JsonValue::of(text->content));
    } else if (const auto* translatable = component.as<Component::Translatable>()) {
        object.set("translate", JsonValue::of(translatable->key));
        if (translatable->fallback) {
            object.set("fallback", JsonValue::of(*translatable->fallback));
        }
        if (!translatable->args.empty()) {
            object.set("with", serializeAll(translatable->args, legacy));
        }
    } else if (const auto* keybind = component.as<Component::Keybind>()) {
        object.set("keybind", JsonValue::of(keybind->key));
    } else if (const auto* score = component.as<Component::Score>()) {
        JsonObject body;
        body.set("name", JsonValue::of(score->name));
        body.set("objective", JsonValue::of(score->objective));
        object.set("score", JsonValue::ofObject(std::move(body)));
    } else {
        const auto& selector = std::get<Component::Selector>(component.payload());
        object.set("selector", JsonValue::of(selector.pattern));
        if (selector.separator) {
            object.set("separator", serializeWith(*selector.separator, legacy));
        }
    }

    writeStyle(object, component.style(), legacy);
    if (!component.children().empty()) {
        object.set("extra", serializeAll(component.children(), legacy));
    }
    return JsonValue::ofObject(std::move(object));
}

Component deserialize(const JsonValue& value) {
    if (const auto* text = value.get<std::string>()) {
        return Component::text(*text);
    }
    if (const auto* array = value.get<std::vector<JsonValue>>()) {
        return deserializeSiblingChain(*array);
    }
    if (const auto* object = value.get<JsonObject>()) {
        return deserializeObject(*object);
    }
    return Component::empty();
}

JsonValue serialize(const Component& component) { return serializeWith(component, false); }

JsonValue serializeLegacy(const Component& component) { return serializeWith(component, true); }

} // namespace basalt::wire::text::componentJson
