#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

/// Bedrock's own NBT, which is not Java's: every number is little-endian, an int is a zigzag varint
/// and a string is a varint length followed by its bytes. The tags are the same tags.
///
/// Written straight into the packet rather than built as a tree: what this server sends is a
/// handful of known shapes, and a tree would be a second representation to keep in step.
namespace basalt::wire::bedrock::nbt {

/// Opens the root, which carries a name in this format even when the name is empty.
void beginCompound(io::ByteBuffer& buffer, std::string_view name = "");

/// Opens a compound inside another one.
void beginNamed(io::ByteBuffer& buffer, std::string_view name);

/// Opens a list of compounds. Each entry is a compound's own fields followed by `end`, and the
/// list itself needs no closing: its length was said in front.
void beginCompoundList(io::ByteBuffer& buffer, std::string_view name, std::int32_t count);

/// A list of strings, whole: unlike a list of compounds it has nothing inside to open.
void writeStringList(io::ByteBuffer& buffer, std::string_view name, std::span<const std::string_view> values);

/// Closes whichever compound is open.
void end(io::ByteBuffer& buffer);

void writeByte(io::ByteBuffer& buffer, std::string_view name, std::int8_t value);

void writeShort(io::ByteBuffer& buffer, std::string_view name, std::int16_t value);

void writeInt(io::ByteBuffer& buffer, std::string_view name, std::int32_t value);

void writeFloat(io::ByteBuffer& buffer, std::string_view name, float value);

void writeString(io::ByteBuffer& buffer, std::string_view name, std::string_view value);

/// One string out of a compound this server did not write, by the path it sits at: `{"FrontText",
/// "Text"}` reaches into the compound named `FrontText` and takes the string called `Text`.
///
/// A reader rather than a tree for the same reason as the writer: this build reads one shape out of
/// one packet, and everything else in it is walked past rather than kept.
[[nodiscard]] std::optional<std::string> readStringAt(io::ByteBuffer& buffer, std::span<const std::string_view> path);

} // namespace basalt::wire::bedrock::nbt
