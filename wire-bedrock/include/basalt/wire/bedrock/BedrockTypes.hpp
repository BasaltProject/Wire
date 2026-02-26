#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::wire::bedrock {

/// Bedrock's primitives. The fixed-width ones are little endian and the variable ones are LEB128,
/// which is the opposite of Java Edition on both counts: nothing here can borrow that codec.
///
/// The buffer's sticky failure flag carries the errors, so none of these return one.

void writeVarUInt(io::ByteBuffer& buffer, std::uint32_t value);

[[nodiscard]] std::uint32_t readVarUInt(io::ByteBuffer& buffer) noexcept;

void writeVarInt(io::ByteBuffer& buffer, std::int32_t value);

[[nodiscard]] std::int32_t readVarInt(io::ByteBuffer& buffer) noexcept;

void writeVarULong(io::ByteBuffer& buffer, std::uint64_t value);

[[nodiscard]] std::uint64_t readVarULong(io::ByteBuffer& buffer) noexcept;

void writeVarLong(io::ByteBuffer& buffer, std::int64_t value);

[[nodiscard]] std::int64_t readVarLong(io::ByteBuffer& buffer) noexcept;

void writeShortLE(io::ByteBuffer& buffer, std::uint16_t value);

[[nodiscard]] std::uint16_t readShortLE(io::ByteBuffer& buffer) noexcept;

void writeIntLE(io::ByteBuffer& buffer, std::int32_t value);

[[nodiscard]] std::int32_t readIntLE(io::ByteBuffer& buffer) noexcept;

void writeLongLE(io::ByteBuffer& buffer, std::int64_t value);

[[nodiscard]] std::int64_t readLongLE(io::ByteBuffer& buffer) noexcept;

void writeFloatLE(io::ByteBuffer& buffer, float value);

[[nodiscard]] float readFloatLE(io::ByteBuffer& buffer) noexcept;

/// A length in unsigned varint bytes, then the UTF-8 itself.
void writeString(io::ByteBuffer& buffer, std::string_view value);

[[nodiscard]] std::string readString(io::ByteBuffer& buffer, std::size_t maxLength = 1U << 20U);

/// A pair of little-endian longs, most significant first, which is how Bedrock writes a uuid.
void writeUuid(io::ByteBuffer& buffer, std::uint64_t high, std::uint64_t low);

} // namespace basalt::wire::bedrock
