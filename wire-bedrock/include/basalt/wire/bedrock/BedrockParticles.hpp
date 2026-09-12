#pragma once

#include <array>
#include <span>
#include <string_view>

namespace basalt::wire::bedrock {

/// One particle, under both its names.
struct ParticleName {
    std::string_view java;
    std::string_view bedrock;
};

/// Every particle this build can name on the other edition, sorted by the Java name.
[[nodiscard]] std::span<const ParticleName> bedrockParticleNames();

/// What a Bedrock client calls this particle, or nothing when it has no name for it there. A
/// particle nobody can draw is not drawn, rather than drawn as something else.
[[nodiscard]] std::string_view bedrockParticleName(std::string_view javaParticle);

} // namespace basalt::wire::bedrock
