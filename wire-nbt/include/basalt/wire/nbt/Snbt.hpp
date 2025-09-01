#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace basalt::wire::nbt {

/// One parsed value plus the index just past it.
struct SnbtValue {
    NbtTag tag;
    std::size_t position = 0;
};

/// Stringified NBT, the human-readable form used in commands (`/give`, `/data`).
///
/// `stringify` produces a self-consistent canonical form (explicit `b`, `s`, `L`, `f` and `d`
/// suffixes, bare integers for TAG_Int) that `parse` reads back exactly. It is not byte-identical to
/// vanilla's own formatter, which the game cannot observe: SNBT is re-parsed into a tag tree, never
/// compared as text.
namespace snbt {

[[nodiscard]] std::string stringify(const NbtTag& tag);

[[nodiscard]] std::expected<NbtTag, NbtError> parse(std::string_view text);

/// Parses one value starting at `from` and leaves whatever follows alone. What a command argument
/// needs: `/summon pig ~ ~ ~ {a:1} extra` has to stop at the closing brace rather than refuse the
/// rest of the line as trailing data.
[[nodiscard]] std::expected<SnbtValue, NbtError> parseFrom(std::string_view text, std::size_t from);

} // namespace snbt

} // namespace basalt::wire::nbt
