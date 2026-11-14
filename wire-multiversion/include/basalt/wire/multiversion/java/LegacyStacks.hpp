#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <cstdint>
#include <optional>

namespace basalt::wire::multiversion::java {

namespace legacyStacks {

void write(io::ByteBuffer& output, const std::optional<protocol::play::WireItemStack>& item, std::int32_t farEnd);

/// How a tag's root is written: below 1.20.2 it carries a name, always empty.
enum class Root : std::uint8_t { kUnnamed, kNamed };

/// A 1.13.2 stack (present flag, varint id, count, tag) as 1.13 has it: short id, count, tag.
[[nodiscard]] bool flaggedToShortId(io::ByteBuffer& input, io::ByteBuffer& output);

[[nodiscard]] bool shortIdToFlagged(io::ByteBuffer& input, io::ByteBuffer& output);

[[nodiscard]] bool copyFlagged(io::ByteBuffer& input, io::ByteBuffer& output, Root from, Root to);

/// copyFlagged between two named roots, as a plain function.
[[nodiscard]] bool copyNamed(io::ByteBuffer& input, io::ByteBuffer& output);

[[nodiscard]] bool copyTag(io::ByteBuffer& input, io::ByteBuffer& output, Root from, Root to);

[[nodiscard]] std::optional<nbt::NbtCompound> tagOf(const protocol::play::WireItemStack& item);

} // namespace legacyStacks

} // namespace basalt::wire::multiversion::java
