#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/MapPackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

/// The map packet in the layout of the version at the far end: its decorations limited to the types
/// that version draws, and its colours to the palette it knows.
namespace mapLayouts {

void write(io::ByteBuffer& output, const protocol::play::MapItemDataPacket& packet, std::int32_t farEnd);

/// The nearest packed colour the far end's palette has to this one, or the same one when it has it.
[[nodiscard]] std::uint8_t colorFor(std::uint8_t packed, std::int32_t farEnd);

} // namespace mapLayouts

} // namespace basalt::wire::multiversion::java
