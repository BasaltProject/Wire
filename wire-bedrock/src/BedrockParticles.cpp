#include <basalt/wire/bedrock/BedrockParticles.hpp>

#include <algorithm>

namespace basalt::wire::bedrock {

std::string_view bedrockParticleName(std::string_view javaParticle) {
    if (!javaParticle.starts_with("minecraft:")) {
        // The table is written with the namespace, because that is how the registry names them.
        return {};
    }
    const std::span<const ParticleName> names = bedrockParticleNames();
    const auto found = std::ranges::lower_bound(names, javaParticle, {}, &ParticleName::java);
    if (found == names.end() || found->java != javaParticle) {
        return {};
    }
    return found->bedrock;
}

} // namespace basalt::wire::bedrock
