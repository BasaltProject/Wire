#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace basalt::wire::text {

class Component;

/// What appears when a player hovers a piece of text. Mirrors Adventure's HoverEvent model.
///
/// `show_item` and `show_entity` carry plain identifiers because the richer types live in
/// server-registry and server-world, which sit above wire-text. The hovered component is held by
/// shared pointer because Component owns a Style and a Style owns a HoverEvent.
struct HoverEvent {
    struct ShowText {
        std::shared_ptr<const Component> value;

        friend bool operator==(const ShowText&, const ShowText&);
    };

    struct ShowItem {
        std::string itemId;
        std::int32_t count = 1;

        friend bool operator==(const ShowItem&, const ShowItem&) = default;
    };

    struct ShowEntity {
        std::string entityType;
        std::string uuid;
        std::shared_ptr<const Component> name;

        friend bool operator==(const ShowEntity&, const ShowEntity&);
    };

    std::variant<ShowText, ShowItem, ShowEntity> payload;

    [[nodiscard]] std::string_view actionId() const noexcept;

    [[nodiscard]] static HoverEvent showText(Component value);

    [[nodiscard]] static HoverEvent showItem(std::string itemId, std::int32_t count = 1) {
        return HoverEvent{ShowItem{std::move(itemId), count}};
    }

    friend bool operator==(const HoverEvent&, const HoverEvent&) = default;
};

} // namespace basalt::wire::text
