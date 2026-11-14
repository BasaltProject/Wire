#pragma once

#include <cstdint>
#include <span>

namespace basalt::wire::multiversion::java {

/// The dimension types and biomes a 1.16.2 server sends, for the servers before it that sent no biome registry.
[[nodiscard]] std::span<const std::uint8_t> defaultCodec1_16_2();

} // namespace basalt::wire::multiversion::java
