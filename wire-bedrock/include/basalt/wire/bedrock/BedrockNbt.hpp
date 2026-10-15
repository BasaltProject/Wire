#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace basalt::wire::bedrock::nbt {

void beginCompound(io::ByteBuffer& buffer, std::string_view name = "");

void beginNamed(io::ByteBuffer& buffer, std::string_view name);

void beginCompoundList(io::ByteBuffer& buffer, std::string_view name, std::int32_t count);

void writeStringList(io::ByteBuffer& buffer, std::string_view name, std::span<const std::string_view> values);

void end(io::ByteBuffer& buffer);

void writeByte(io::ByteBuffer& buffer, std::string_view name, std::int8_t value);

void writeShort(io::ByteBuffer& buffer, std::string_view name, std::int16_t value);

void writeInt(io::ByteBuffer& buffer, std::string_view name, std::int32_t value);

void writeFloat(io::ByteBuffer& buffer, std::string_view name, float value);

void writeString(io::ByteBuffer& buffer, std::string_view name, std::string_view value);

[[nodiscard]] std::optional<std::string> readStringAt(io::ByteBuffer& buffer, std::span<const std::string_view> path);

} // namespace basalt::wire::bedrock::nbt
