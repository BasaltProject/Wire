#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <cstdint>
#include <cstdlib>

namespace basalt::wire::text::componentNbt {
nbt::NbtTag serializeWith(const Component& component, bool legacy);

namespace {

using nbt::NbtCompound;
using nbt::NbtList;
using nbt::NbtTag;
using nbt::NbtType;

NbtList serializeAll(const std::vector<Component>& components, bool legacy) {
    std::vector<NbtTag> tags;
    tags.reserve(components.size());
    for (const Component& component : components) {
        tags.push_back(serializeWith(component, legacy));
    }
    return NbtList{NbtType::Compound, std::move(tags)};
}

void writeStyle(NbtCompound& compound, const Style& style, bool legacy) {
    if (style.color) {
        const std::optional<std::string_view> name = TextColor::nameOf(*style.color);
        compound.putString("color", name ? std::string{*name} : style.color->toHexString());
    }
    if (style.bold) {
        compound.putBoolean("bold", *style.bold);
    }
    if (style.italic) {
        compound.putBoolean("italic", *style.italic);
    }
    if (style.underlined) {
        compound.putBoolean("underlined", *style.underlined);
    }
    if (style.strikethrough) {
        compound.putBoolean("strikethrough", *style.strikethrough);
    }
    if (style.obfuscated) {
        compound.putBoolean("obfuscated", *style.obfuscated);
    }
    if (style.font) {
        compound.putString("font", *style.font);
    }
    if (style.insertion) {
        compound.putString("insertion", *style.insertion);
    }
    if (legacy) {
        if (style.clickEvent) {
            NbtCompound click;
            click.putString("action", std::string{clickActionId(style.clickEvent->action)});
            click.putString("value", style.clickEvent->value);
            compound.set("clickEvent", NbtTag::ofCompound(std::move(click)));
        }
        if (style.hoverEvent) {
            NbtCompound hover;
            hover.putString("action", std::string{style.hoverEvent->actionId()});
            if (const auto* showText = std::get_if<HoverEvent::ShowText>(&style.hoverEvent->payload)) {
                hover.set("contents",
                          showText->value ? serializeWith(*showText->value, true) : NbtTag::ofCompound(NbtCompound{}));
            } else if (const auto* showItem = std::get_if<HoverEvent::ShowItem>(&style.hoverEvent->payload)) {
                NbtCompound contents;
                contents.putString("id", showItem->itemId);
                contents.putInt("count", showItem->count);
                hover.set("contents", NbtTag::ofCompound(std::move(contents)));
            } else {
                const auto& showEntity = std::get<HoverEvent::ShowEntity>(style.hoverEvent->payload);
                NbtCompound contents;
                contents.putString("type", showEntity.entityType);
                contents.putString("id", showEntity.uuid);
                if (showEntity.name) {
                    contents.set("name", serializeWith(*showEntity.name, true));
                }
                hover.set("contents", NbtTag::ofCompound(std::move(contents)));
            }
            compound.set("hoverEvent", NbtTag::ofCompound(std::move(hover)));
        }
        return;
    }
    if (style.clickEvent) {
        const ClickEvent& event = *style.clickEvent;
        NbtCompound click;
        click.putString("action", std::string{clickActionId(event.action)});
        switch (event.action) {
        case ClickEvent::Action::OpenUrl:
            click.putString("url", event.value);
            break;
        case ClickEvent::Action::RunCommand:
        case ClickEvent::Action::SuggestCommand:
            click.putString("command", event.value);
            break;
        case ClickEvent::Action::ChangePage:
            click.putInt("page", std::atoi(event.value.c_str()));
            break;
        case ClickEvent::Action::CopyToClipboard:
            click.putString("value", event.value);
            break;
        }
        compound.set("click_event", NbtTag::ofCompound(std::move(click)));
    }
    if (style.hoverEvent) {
        NbtCompound hover;
        hover.putString("action", std::string{style.hoverEvent->actionId()});
        if (const auto* showText = std::get_if<HoverEvent::ShowText>(&style.hoverEvent->payload)) {
            hover.set("value",
                      showText->value ? serializeWith(*showText->value, legacy) : NbtTag::ofCompound(NbtCompound{}));
        } else if (const auto* showItem = std::get_if<HoverEvent::ShowItem>(&style.hoverEvent->payload)) {
            hover.putString("id", showItem->itemId);
            hover.putInt("count", showItem->count);
        } else {
            const auto& showEntity = std::get<HoverEvent::ShowEntity>(style.hoverEvent->payload);
            hover.putString("id", showEntity.entityType);
            if (const std::optional<common::Uuid> uuid = common::Uuid::parse(showEntity.uuid)) {
                hover.putIntArray(
                    "uuid", {static_cast<std::int32_t>(uuid->high() >> 32), static_cast<std::int32_t>(uuid->high()),
                             static_cast<std::int32_t>(uuid->low() >> 32), static_cast<std::int32_t>(uuid->low())});
            }
            if (showEntity.name) {
                hover.set("name", serializeWith(*showEntity.name, legacy));
            }
        }
        compound.set("hover_event", NbtTag::ofCompound(std::move(hover)));
    }
}

std::optional<bool> readBoolean(const NbtCompound& compound, std::string_view key) {
    const NbtTag* tag = compound.find(key);
    if (tag == nullptr) {
        return std::nullopt;
    }
    const auto* value = tag->get<std::int8_t>();
    return value != nullptr ? std::optional<bool>{*value != 0} : std::nullopt;
}

std::optional<std::string> readString(const NbtCompound& compound, std::string_view key) {
    const NbtTag* tag = compound.find(key);
    if (tag == nullptr) {
        return std::nullopt;
    }
    const auto* value = tag->get<std::string>();
    return value != nullptr ? std::optional<std::string>{*value} : std::nullopt;
}

std::optional<ClickEvent> readClickEvent(const NbtCompound& compound) {
    const std::optional<ClickEvent::Action> action = clickActionFromId(compound.getString("action"));
    if (!action) {
        return std::nullopt;
    }
    if (*action == ClickEvent::Action::ChangePage) {
        if (const NbtTag* page = compound.find("page"); page != nullptr && page->get<std::int32_t>() != nullptr) {
            return ClickEvent::changePage(*page->get<std::int32_t>());
        }
    }
    for (const std::string_view key : {"url", "command", "value"}) {
        if (const std::optional<std::string> value = readString(compound, key)) {
            return ClickEvent{*action, *value};
        }
    }
    return std::nullopt;
}

std::optional<HoverEvent> readHoverEvent(const NbtCompound& compound) {
    const std::string_view action = compound.getString("action");
    const NbtTag* contents = compound.find("contents");
    if (action == "show_text") {
        const NbtTag* value = compound.find("value");
        if (value == nullptr) {
            value = contents;
        }
        return value != nullptr ? std::optional<HoverEvent>{HoverEvent::showText(deserialize(*value))} : std::nullopt;
    }
    const NbtCompound* body = contents != nullptr ? contents->get<NbtCompound>() : &compound;
    if (body == nullptr) {
        return std::nullopt;
    }
    if (action == "show_item") {
        return HoverEvent::showItem(std::string{body->getString("id")}, body->getInt("count", 1));
    }
    if (action == "show_entity") {
        const bool legacy = body != &compound;
        HoverEvent::ShowEntity entity{std::string{body->getString(legacy ? "type" : "id")}, "", nullptr};
        if (legacy) {
            entity.uuid = std::string{body->getString("id")};
        } else if (const auto ints = body->getIntArray("uuid"); ints.size() == 4) {
            const auto word = [&](std::size_t i) {
                return static_cast<std::uint64_t>(static_cast<std::uint32_t>(ints[i]));
            };
            entity.uuid = common::Uuid{(word(0) << 32) | word(1), (word(2) << 32) | word(3)}.toString();
        }
        if (const NbtTag* name = body->find("name")) {
            entity.name = std::make_shared<const Component>(deserialize(*name));
        }
        return HoverEvent{std::move(entity)};
    }
    return std::nullopt;
}

Style readStyle(const NbtCompound& compound) {
    Style style;
    if (const std::optional<std::string> color = readString(compound, "color")) {
        style.color = TextColor::parse(*color);
    }
    style.bold = readBoolean(compound, "bold");
    style.italic = readBoolean(compound, "italic");
    style.underlined = readBoolean(compound, "underlined");
    style.strikethrough = readBoolean(compound, "strikethrough");
    style.obfuscated = readBoolean(compound, "obfuscated");
    style.font = readString(compound, "font");
    style.insertion = readString(compound, "insertion");
    for (const std::string_view key : {"click_event", "clickEvent"}) {
        if (const NbtCompound* click = compound.getCompound(key)) {
            style.clickEvent = readClickEvent(*click);
            break;
        }
    }
    for (const std::string_view key : {"hover_event", "hoverEvent"}) {
        if (const NbtCompound* hover = compound.getCompound(key)) {
            style.hoverEvent = readHoverEvent(*hover);
            break;
        }
    }
    return style;
}

std::vector<Component> deserializeAll(const NbtList* list) {
    std::vector<Component> out;
    if (list == nullptr) {
        return out;
    }
    out.reserve(list->size());
    for (const NbtTag& element : *list) {
        out.push_back(deserialize(element));
    }
    return out;
}

Component deserializeCompound(const NbtCompound& compound) {
    Component::Payload payload = Component::Text{};
    if (compound.contains("text")) {
        payload = Component::Text{std::string{compound.getString("text")}};
    } else if (compound.contains("translate")) {
        payload = Component::Translatable{std::string{compound.getString("translate")},
                                          deserializeAll(compound.getList("with")), readString(compound, "fallback")};
    } else if (compound.contains("keybind")) {
        payload = Component::Keybind{std::string{compound.getString("keybind")}};
    } else if (const NbtCompound* score = compound.getCompound("score")) {
        payload = Component::Score{std::string{score->getString("name")}, std::string{score->getString("objective")}};
    } else if (compound.contains("selector")) {
        Component::Selector selector{std::string{compound.getString("selector")}, nullptr};
        if (const NbtTag* separator = compound.find("separator")) {
            selector.separator = std::make_shared<const Component>(deserialize(*separator));
        }
        payload = std::move(selector);
    }

    return Component{std::move(payload), readStyle(compound), deserializeAll(compound.getList("extra"))};
}

Component deserializeSiblingChain(const NbtList& list) {
    if (list.empty()) {
        return Component::empty();
    }
    Component result = deserialize(list[0]);
    for (std::size_t i = 1; i < list.size(); ++i) {
        result = result.append(deserialize(list[i]));
    }
    return result;
}

} // namespace

NbtTag serializeWith(const Component& component, bool legacy) {
    NbtCompound compound;
    if (const auto* text = component.as<Component::Text>()) {
        compound.putString("text", text->content);
    } else if (const auto* translatable = component.as<Component::Translatable>()) {
        compound.putString("translate", translatable->key);
        if (translatable->fallback) {
            compound.putString("fallback", *translatable->fallback);
        }
        if (!translatable->args.empty()) {
            compound.set("with", NbtTag::ofList(serializeAll(translatable->args, legacy)));
        }
    } else if (const auto* keybind = component.as<Component::Keybind>()) {
        compound.putString("keybind", keybind->key);
    } else if (const auto* score = component.as<Component::Score>()) {
        NbtCompound body;
        body.putString("name", score->name);
        body.putString("objective", score->objective);
        compound.set("score", NbtTag::ofCompound(std::move(body)));
    } else {
        const auto& selector = std::get<Component::Selector>(component.payload());
        compound.putString("selector", selector.pattern);
        if (selector.separator) {
            compound.set("separator", serializeWith(*selector.separator, legacy));
        }
    }

    writeStyle(compound, component.style(), legacy);
    if (!component.children().empty()) {
        compound.set("extra", NbtTag::ofList(serializeAll(component.children(), legacy)));
    }
    return NbtTag::ofCompound(std::move(compound));
}

Component deserialize(const NbtTag& tag) {
    if (const auto* text = tag.get<std::string>()) {
        return Component::text(*text);
    }
    if (const auto* list = tag.get<NbtList>()) {
        return deserializeSiblingChain(*list);
    }
    if (const auto* compound = tag.get<NbtCompound>()) {
        return deserializeCompound(*compound);
    }
    return Component::empty();
}

namespace {
thread_local bool legacyEvents = false;
} // namespace

ScopedLegacyEvents::ScopedLegacyEvents() noexcept : previous_(legacyEvents) { legacyEvents = true; }

ScopedLegacyEvents::~ScopedLegacyEvents() { legacyEvents = previous_; }

NbtTag serialize(const Component& component) { return serializeWith(component, legacyEvents); }

NbtTag serializeLegacy(const Component& component) { return serializeWith(component, true); }

} // namespace basalt::wire::text::componentNbt
