#pragma once

#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/Component.hpp>

namespace basalt::wire::text::componentJson {

[[nodiscard]] common::JsonValue serialize(const Component& component);

[[nodiscard]] Component deserialize(const common::JsonValue& value);

} // namespace basalt::wire::text::componentJson
