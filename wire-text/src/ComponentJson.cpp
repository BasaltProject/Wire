#include <basalt/wire/text/ComponentJson.hpp>

#include <cstdint>

namespace basalt::wire::text::componentJson {
namespace {

using common::JsonObject;
using common::JsonValue;

JsonValue serializeAll(const std::vector<Component>& components) {
    std::vector<JsonValue> out;
    out.reserve(components.size());
    for (const Component& component : components) {
        out.push_back(serialize(component));
    }
    return JsonValue::ofArray(std::move(out));
}

void writeStyle(JsonObject& object, const Style& style) {
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
            hover.set("contents", showText->value ? serialize(*showText->value) : JsonValue{});
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
                contents.set("name", serialize(*showEntity.name));
            }
            hover.set("contents", JsonValue::ofObject(std::move(contents)));
        }
        object.set("hoverEvent", JsonValue::ofObject(std::move(hover)));
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
    const std::optional<std::string> value = readString(object, "value");
    if (!action || !value) {
        return std::nullopt;
    }
    const std::optional<ClickEvent::Action> parsed = clickActionFromId(*action);
    if (!parsed) {
        return std::nullopt;
    }
    return ClickEvent{*parsed, *value};
}

std::optional<HoverEvent> readHoverEvent(const JsonObject& object) {
    const std::optional<std::string> action = readString(object, "action");
    const JsonValue* contents = object.find("contents");
    if (!action || contents == nullptr) {
        return std::nullopt;
    }
    if (*action == "show_text") {
        return HoverEvent::showText(deserialize(*contents));
    }
    const auto* body = contents->get<JsonObject>();
    if (body == nullptr) {
        return std::nullopt;
    }
    if (*action == "show_item") {
        return HoverEvent::showItem(readString(*body, "id").value_or(""), readInt(*body, "count", 1));
    }
    if (*action == "show_entity") {
        HoverEvent::ShowEntity entity{readString(*body, "type").value_or(""), readString(*body, "id").value_or(""),
                                      nullptr};
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
    if (const JsonObject* click = readObject(object, "clickEvent")) {
        style.clickEvent = readClickEvent(*click);
    }
    if (const JsonObject* hover = readObject(object, "hoverEvent")) {
        style.hoverEvent = readHoverEvent(*hover);
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

JsonValue serialize(const Component& component) {
    JsonObject object;
    if (const auto* text = component.as<Component::Text>()) {
        object.set("text", JsonValue::of(text->content));
    } else if (const auto* translatable = component.as<Component::Translatable>()) {
        object.set("translate", JsonValue::of(translatable->key));
        if (translatable->fallback) {
            object.set("fallback", JsonValue::of(*translatable->fallback));
        }
        if (!translatable->args.empty()) {
            object.set("with", serializeAll(translatable->args));
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
            object.set("separator", serialize(*selector.separator));
        }
    }

    writeStyle(object, component.style());
    if (!component.children().empty()) {
        object.set("extra", serializeAll(component.children()));
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

} // namespace basalt::wire::text::componentJson
