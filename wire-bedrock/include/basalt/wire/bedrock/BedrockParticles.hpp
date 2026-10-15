#pragma once

#include <array>
#include <span>
#include <string_view>

namespace basalt::wire::bedrock {

struct ParticleName {
    std::string_view java;
    std::string_view bedrock;
};

[[nodiscard]] std::span<const ParticleName> bedrockParticleNames();

/// What a Bedrock client calls this particle, or nothing when it has no name for it there.
[[nodiscard]] std::string_view bedrockParticleName(std::string_view javaParticle);

} // namespace basalt::wire::bedrock
