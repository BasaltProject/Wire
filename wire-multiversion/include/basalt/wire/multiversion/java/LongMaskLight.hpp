#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <optional>

namespace basalt::wire::multiversion::java {

void writeLongMaskLight(io::ByteBuffer& output, const protocol::play::LightData& light);

[[nodiscard]] std::optional<protocol::play::LightData> readLongMaskLight(io::ByteBuffer& input);

} // namespace basalt::wire::multiversion::java
