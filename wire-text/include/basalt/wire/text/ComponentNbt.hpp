#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/text/Component.hpp>

namespace basalt::wire::text::componentNbt {

[[nodiscard]] nbt::NbtTag serialize(const Component& component);

[[nodiscard]] Component deserialize(const nbt::NbtTag& tag);

} // namespace basalt::wire::text::componentNbt
