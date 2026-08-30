#pragma once

#include <basalt/wire/text/Component.hpp>

#include <span>
#include <string_view>

namespace basalt::wire::text::miniMessage {

/// Parser for the MiniMessage tag markup (`<red>Hello</red>`), the format Basalt plugins author text
/// in. Covers colours (named and `#rrggbb`), decorations, `<reset>`, `<click:...>`,
/// `<hover:show_text:...>` and two-or-more-stop `<gradient:...>`, applied to the literal text up to
/// the matching `</gradient>` without recursing into tags inside it.
///
/// Not implemented: `<rainbow>`, `<transition>`, `<insert>`, `<keybind>`, `<translate>`, and escaped
/// quotes inside tag arguments.
[[nodiscard]] Component parse(std::string_view input);

/// Applies a linear colour gradient across the text, one stop per character.
[[nodiscard]] Component gradient(std::string_view text, std::span<const TextColor> stops);

} // namespace basalt::wire::text::miniMessage
