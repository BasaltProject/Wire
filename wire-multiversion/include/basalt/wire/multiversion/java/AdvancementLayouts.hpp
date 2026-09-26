#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/AdvancementPackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

/// The advancements packet in the layout of the version at the far end, ids already its own.
///
/// Written whole rather than link by link: after the item stack turns into the old tagged shape,
/// nothing above 1.20.5 could read the packet back to move the next field.
namespace advancementLayouts {

void write(io::ByteBuffer& output, const protocol::play::UpdateAdvancementsPacket& packet, std::int32_t farEnd);

} // namespace advancementLayouts

} // namespace basalt::wire::multiversion::java
