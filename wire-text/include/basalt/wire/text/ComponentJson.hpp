#pragma once

#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/Component.hpp>

namespace basalt::wire::text::componentJson {

/// JSON serialization in vanilla's modern component format: nested `clickEvent` and `hoverEvent`
/// objects, `extra` for children. Built on a JSON tree rather than generated codecs because the
/// shape is polymorphic by which key is present and every style field is tri-state.
[[nodiscard]] common::JsonValue serialize(const Component& component);

[[nodiscard]] Component deserialize(const common::JsonValue& value);

} // namespace basalt::wire::text::componentJson
