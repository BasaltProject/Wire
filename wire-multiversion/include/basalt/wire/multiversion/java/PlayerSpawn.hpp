#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

/// Rewrites a spawn-player packet (entity id, uuid, position, yaw, pitch) as a spawn-entity one of that type.
void playerAsEntity(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t playerType);

} // namespace basalt::wire::multiversion::java
