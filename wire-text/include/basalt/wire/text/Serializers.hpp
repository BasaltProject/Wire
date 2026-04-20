#pragma once

#include <basalt/wire/text/Component.hpp>

#include <string>

namespace basalt::wire::text {

[[nodiscard]] std::string toPlainText(const Component& component);

[[nodiscard]] std::string toAnsi(const Component& component);

/// Legacy codes belong at the edge of a connection only and are never read back inside the server.
[[nodiscard]] std::string toLegacyText(const Component& component);

} // namespace basalt::wire::text
