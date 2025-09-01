#pragma once

#include <basalt/wire/text/ClickEvent.hpp>
#include <basalt/wire/text/HoverEvent.hpp>
#include <basalt/wire/text/TextColor.hpp>

#include <optional>
#include <string>

namespace basalt::wire::text {

/// Formatting for a Component. Every field is tri-state: absent means "inherited from the parent",
/// not "off". That is vanilla's actual inheritance model, so a bold parent with a plain child still
/// renders bold unless the child explicitly turns it off.
struct Style {
    std::optional<TextColor> color;
    std::optional<bool> bold;
    std::optional<bool> italic;
    std::optional<bool> underlined;
    std::optional<bool> strikethrough;
    std::optional<bool> obfuscated;
    std::optional<std::string> font;
    std::optional<std::string> insertion;
    std::optional<ClickEvent> clickEvent;
    std::optional<HoverEvent> hoverEvent;

    [[nodiscard]] bool isEmpty() const noexcept {
        return !color && !bold && !italic && !underlined && !strikethrough && !obfuscated && !font && !insertion &&
               !clickEvent && !hoverEvent;
    }

    /// Every field of `other` overlaid on this one, wherever `other` sets it.
    [[nodiscard]] Style merge(const Style& other) const;

    friend bool operator==(const Style&, const Style&) = default;
};

} // namespace basalt::wire::text
