#pragma once

#include <basalt/wire/protocol/play/ParticleOptions.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java::nativeParticles {

/// What options follow a particle of this type, by this server's numbering.
[[nodiscard]] protocol::play::ParticleOptionShape shapeOf(std::int32_t nativeType);

} // namespace basalt::wire::multiversion::java::nativeParticles
