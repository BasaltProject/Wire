#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <cstdint>
#include <optional>

namespace basalt::wire::multiversion::java {

/// A stack in the shape its own version reads one: components from 1.20.5, an item, count and tag
/// below that, and a short with its damage below 1.13. The id is already the far end's by then.
namespace legacyStacks {

void write(io::ByteBuffer& output, const std::optional<protocol::play::WireItemStack>& item, std::int32_t farEnd);

/// The two components an old tag has a name for, and absent when the stack carries neither.
[[nodiscard]] std::optional<nbt::NbtCompound> tagOf(const protocol::play::WireItemStack& item);

} // namespace legacyStacks

} // namespace basalt::wire::multiversion::java
