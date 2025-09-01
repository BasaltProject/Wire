#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/text/Component.hpp>

namespace basalt::wire::text::componentNbt {

/// NBT serialization in the modern network component format: the same shape as the JSON form, but
/// booleans are TAG_Byte, which is what the play-protocol chat packets actually carry.
[[nodiscard]] nbt::NbtTag serialize(const Component& component);

[[nodiscard]] Component deserialize(const nbt::NbtTag& tag);

} // namespace basalt::wire::text::componentNbt
