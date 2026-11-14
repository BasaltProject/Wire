#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::wire::multiversion::java::namedBlockEntities {

inline constexpr std::uint8_t kNoAction = 0;

/// A block entity as 1.17.1 and earlier read it: its own position and registry name inside the tag.
[[nodiscard]] nbt::NbtCompound tagFor(std::int32_t nativeType, const nbt::NbtCompound* tag, std::int32_t x,
                                      std::int32_t y, std::int32_t z);

/// Before 1.21.5 the lines of a sign were json strings rather than components; true when the tag was a sign.
bool signLinesAsJson(nbt::NbtCompound& tag);

/// Before 1.20 a sign had one side, kept as four json lines with their colour and glow beside them.
bool signAsOneSide(nbt::NbtCompound& tag);

/// A position as packed before 1.14: x, then y, then z.
[[nodiscard]] constexpr std::int64_t packBefore1_14(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
    const auto bits = ((static_cast<std::uint64_t>(x) & 0x3FFFFFFULL) << 38) |
                      ((static_cast<std::uint64_t>(y) & 0xFFFULL) << 26) |
                      (static_cast<std::uint64_t>(z) & 0x3FFFFFFULL);
    return static_cast<std::int64_t>(bits);
}

/// The action byte a block entity data packet carried for this type up to 1.17.1, or `kNoAction`.
[[nodiscard]] std::uint8_t actionFor(std::string_view name);

/// Before 1.11 a block entity went by a name of its own rather than its registry name.
void useNameBefore1_11(nbt::NbtCompound& tag);

inline constexpr std::string_view kSignBefore1_11 = "Sign";

} // namespace basalt::wire::multiversion::java::namedBlockEntities
