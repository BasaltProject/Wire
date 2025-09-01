#pragma once

#include "BinaryIO.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace basalt::wire::nbt::modifiedUtf8 {

/// NBT tag names and string payloads use Java's "modified UTF-8" (`DataOutput.writeUTF`), not plain
/// UTF-8: the null character takes two bytes instead of one, and a supplementary character is
/// written as two 3-byte sequences, one per UTF-16 surrogate half, instead of one 4-byte sequence.
/// Reading Mojang-written NBT as plain UTF-8 silently corrupts any string that contains either, so
/// the conversion is spelled out here instead of borrowing the standard one.

[[nodiscard]] std::size_t encodedLength(std::string_view value) noexcept;

void encode(std::string_view value, ByteWriter& sink);

[[nodiscard]] std::expected<std::string, NbtError> decode(ByteReader& source, std::size_t byteLength);

} // namespace basalt::wire::nbt::modifiedUtf8
