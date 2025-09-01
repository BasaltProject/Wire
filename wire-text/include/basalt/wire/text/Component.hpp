#pragma once

#include <basalt/wire/text/Style.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::text {

/// An immutable, styled piece of text, mirroring Adventure's Component model. Every public API that
/// shows text to a player takes one of these, never a formatted string: legacy `§` codes are not an
/// internal representation anywhere in Basalt.
///
/// The `nbt` and `object` variants (dynamic `/data`-query text, embedded map-icon text) are not
/// implemented; both are rare enough to defer.
class Component {
public:
    struct Text {
        std::string content;

        friend bool operator==(const Text&, const Text&) = default;
    };

    struct Translatable {
        std::string key;
        std::vector<Component> args;
        std::optional<std::string> fallback;

        friend bool operator==(const Translatable&, const Translatable&);
    };

    struct Keybind {
        std::string key;

        friend bool operator==(const Keybind&, const Keybind&) = default;
    };

    struct Score {
        std::string name;
        std::string objective;

        friend bool operator==(const Score&, const Score&) = default;
    };

    struct Selector {
        std::string pattern;
        std::shared_ptr<const Component> separator;

        friend bool operator==(const Selector&, const Selector&);
    };

    using Payload = std::variant<Text, Translatable, Keybind, Score, Selector>;

    Component();
    explicit Component(Payload payload);
    Component(Payload payload, Style style, std::vector<Component> children);
    Component(const Component&);
    Component(Component&&) noexcept;
    Component& operator=(const Component&);
    Component& operator=(Component&&) noexcept;
    ~Component();

    [[nodiscard]] static Component text(std::string content);
    [[nodiscard]] static Component text(std::string content, TextColor color);
    [[nodiscard]] static Component translatable(std::string key, std::vector<Component> args = {});
    [[nodiscard]] static Component keybind(std::string key);
    [[nodiscard]] static Component score(std::string name, std::string objective);
    [[nodiscard]] static Component empty();

    /// Interleaves `separator` between the components, as one parentless text node holding them all.
    [[nodiscard]] static Component join(const Component& separator, const std::vector<Component>& components);

    [[nodiscard]] const Payload& payload() const noexcept { return payload_; }

    [[nodiscard]] const Style& style() const noexcept { return style_; }

    [[nodiscard]] const std::vector<Component>& children() const noexcept { return children_; }

    template <typename T>
    [[nodiscard]] const T* as() const noexcept {
        return std::get_if<T>(&payload_);
    }

    [[nodiscard]] Component withStyle(Style newStyle) const;
    [[nodiscard]] Component withChildren(std::vector<Component> newChildren) const;
    [[nodiscard]] Component append(Component child) const;

    [[nodiscard]] Component color(std::optional<TextColor> value) const;
    [[nodiscard]] Component bold(bool value = true) const;
    [[nodiscard]] Component italic(bool value = true) const;
    [[nodiscard]] Component underlined(bool value = true) const;
    [[nodiscard]] Component strikethrough(bool value = true) const;
    [[nodiscard]] Component obfuscated(bool value = true) const;
    [[nodiscard]] Component clickEvent(std::optional<ClickEvent> event) const;
    [[nodiscard]] Component hoverEvent(std::optional<HoverEvent> event) const;

    friend bool operator==(const Component&, const Component&);

private:
    Payload payload_;
    Style style_;
    std::vector<Component> children_;
};

} // namespace basalt::wire::text
