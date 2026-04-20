#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/AdvancementPackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

namespace advancementLayouts {

void write(io::ByteBuffer& output, const protocol::play::UpdateAdvancementsPacket& packet, std::int32_t farEnd);

}

} // namespace basalt::wire::multiversion::java
