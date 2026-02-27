#pragma once

#include <cstdint>

namespace basalt::wire::bedrock {

/// The Bedrock number for a Java mob effect id, or zero when that edition has no such effect.
///
/// The two registries agree up to `saturation` and then stop: Java's `glowing` has no Bedrock
/// twin, so every number after it is shifted, and four effects have no answer at all.
[[nodiscard]] std::int32_t bedrockEffectId(std::int32_t javaEffect);

} // namespace basalt::wire::bedrock
