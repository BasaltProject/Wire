#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <cstdint>
#include <optional>

namespace basalt::wire::multiversion::java {

namespace legacyStacks {

void write(io::ByteBuffer& output, const std::optional<protocol::play::WireItemStack>& item, std::int32_t farEnd);

[[nodiscard]] std::optional<nbt::NbtCompound> tagOf(const protocol::play::WireItemStack& item);

} // namespace legacyStacks

} // namespace basalt::wire::multiversion::java
