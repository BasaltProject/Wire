#pragma once

#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <expected>

namespace basalt::wire::nbt {

/// Why a JSON document had a shape NBT cannot hold without changing what it says.
enum class JsonToNbtError : std::uint8_t {
    NullHasNoEquivalent,
    MixedArray,
};

/// Datapack JSON as NBT, which is the form a registry entry travels in.
///
/// Mojang's codecs read numbers through NumericTag, so the numeric type is forgiving; the shape is
/// not. A list's elements must all share a type, the one rule JSON does not enforce and NBT does. A
/// boolean becomes a byte.
[[nodiscard]] std::expected<NbtTag, JsonToNbtError> jsonToNbt(const common::JsonValue& value);

} // namespace basalt::wire::nbt
