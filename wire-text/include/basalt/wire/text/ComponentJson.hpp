#pragma once

#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/Component.hpp>

namespace basalt::wire::text::componentJson {

[[nodiscard]] common::JsonValue serialize(const Component& component);

/// The pre-1.21.5 shape of click and hover events (`clickEvent`/`hoverEvent`), for older clients.
[[nodiscard]] common::JsonValue serializeLegacy(const Component& component);

[[nodiscard]] Component deserialize(const common::JsonValue& value);

} // namespace basalt::wire::text::componentJson
