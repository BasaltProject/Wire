#pragma once

#include "BinaryIO.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace basalt::wire::nbt::modifiedUtf8 {

[[nodiscard]] std::size_t encodedLength(std::string_view value) noexcept;

void encode(std::string_view value, ByteWriter& sink);

[[nodiscard]] std::expected<std::string, NbtError> decode(ByteReader& source, std::size_t byteLength);

} // namespace basalt::wire::nbt::modifiedUtf8
