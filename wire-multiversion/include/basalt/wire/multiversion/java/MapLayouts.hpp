#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/MapPackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

namespace mapLayouts {

void write(io::ByteBuffer& output, const protocol::play::MapItemDataPacket& packet, std::int32_t farEnd);

[[nodiscard]] std::uint8_t colorFor(std::uint8_t packed, std::int32_t farEnd);

} // namespace mapLayouts

} // namespace basalt::wire::multiversion::java
