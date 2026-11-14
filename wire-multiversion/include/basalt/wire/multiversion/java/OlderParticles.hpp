#pragma once

#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::wire::multiversion::java::olderParticles {

/// The native id the finished particle travels under, which no link reshapes again.
inline constexpr std::int32_t kTranslatedParticles = 0x7F11;

/// A particle as 1.13 to 1.21.11 read it, an item in it a whole stack rather than a template, with `type`
/// already the far end's, or nothing when that era cannot draw it.
[[nodiscard]] std::optional<std::vector<std::uint8_t>>
write(const protocol::play::LevelParticlesPacket& particle, std::int32_t type, std::int32_t farEnd,
      const std::function<std::int32_t(std::int32_t)>& blockState,
      const std::function<void(io::ByteBuffer&, const protocol::play::ParticleOptions::Item&)>& item);

} // namespace basalt::wire::multiversion::java::olderParticles
