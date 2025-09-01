#pragma once

#include <basalt/wire/text/Component.hpp>

#include <string>

namespace basalt::wire::text {

/// Flattens a component tree to a plain string, dropping all styling.
[[nodiscard]] std::string toPlainText(const Component& component);

/// Renders a component tree as ANSI-escaped text, for console output.
[[nodiscard]] std::string toAnsi(const Component& component);

} // namespace basalt::wire::text
