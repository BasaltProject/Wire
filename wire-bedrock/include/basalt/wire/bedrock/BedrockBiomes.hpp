#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace basalt::wire::bedrock {

/// A vanilla biome carries no id of its own: the client knows it by name.
struct BedrockBiomeDefinition {
    std::string_view name;
    float temperature = 0.0F;
    float downfall = 0.0F;
    float foliageSnow = 0.0F;
    float depth = 0.0F;
    float scale = 0.0F;
    std::uint32_t mapWaterColour = 0;
    bool rain = false;
    bool hasTags = false;
    std::uint16_t firstTag = 0;
    std::uint16_t tagCount = 0;
};

[[nodiscard]] std::span<const BedrockBiomeDefinition> bedrockBiomeDefinitions();

[[nodiscard]] std::span<const std::string_view> bedrockBiomeTags();

[[nodiscard]] std::span<const std::uint16_t> bedrockBiomeTagsOf(const BedrockBiomeDefinition& definition);

[[nodiscard]] std::optional<std::int32_t> bedrockBiomeId(std::string_view javaBiome);

/// parity: Geyser's overworld fallback, `ocean`.
inline constexpr std::int32_t kFallbackBedrockBiome = 0;

/// Biomes in one section, 4x4x4 in Java's `(y * 4 + z) * 4 + x` order, as Bedrock ids.
using BiomeQuarts = std::array<std::int32_t, 64>;

void writeBiomeSection(io::ByteBuffer& buffer, const BiomeQuarts& quarts);

} // namespace basalt::wire::bedrock
