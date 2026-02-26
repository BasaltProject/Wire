#pragma once

#include <basalt/wire/text/Component.hpp>

#include <string>

namespace basalt::wire::text {

/// Flattens a component tree to a plain string, dropping all styling.
[[nodiscard]] std::string toPlainText(const Component& component);

/// Renders a component tree as ANSI-escaped text, for console output.
[[nodiscard]] std::string toAnsi(const Component& component);

/// Renders a component tree the old way, with `§` codes. The explicit legacy serializer, written at
/// the edge of a connection and never held or read back inside the server: Bedrock and the oldest
/// Java versions read nothing else. A truecolor outside the sixteen becomes the nearest one.
[[nodiscard]] std::string toLegacyText(const Component& component);

} // namespace basalt::wire::text
