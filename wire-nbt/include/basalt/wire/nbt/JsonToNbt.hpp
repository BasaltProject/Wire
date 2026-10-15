#pragma once

#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <expected>

namespace basalt::wire::nbt {

enum class JsonToNbtError : std::uint8_t {
    NullHasNoEquivalent,
    MixedArray,
};

/// A list's elements must share a type, which NBT enforces and JSON does not; a boolean becomes a byte.
[[nodiscard]] std::expected<NbtTag, JsonToNbtError> jsonToNbt(const common::JsonValue& value);

} // namespace basalt::wire::nbt
