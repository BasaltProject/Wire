#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <optional>

namespace basalt::wire::multiversion::java {

/// The light half of a chunk as 26.2 and below write it: each mask a list of longs rather than
/// 26.3's bytes.
void writeLongMaskLight(io::ByteBuffer& output, const protocol::play::LightData& light);

[[nodiscard]] std::optional<protocol::play::LightData> readLongMaskLight(io::ByteBuffer& input);

} // namespace basalt::wire::multiversion::java
