#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>

namespace basalt::wire::multiversion::java::olderBlockEntities {

/// Gives a skull, banner or spawner tag the keys its block entity read before 1.20.5, by this server's type id.
/// `nativeState` is the block under it, which before 1.13 told a head's kind and facing and a banner's colour.
void rewrite(std::int32_t nativeType, nbt::NbtCompound& tag, std::int32_t farEnd,
             std::optional<std::int32_t> nativeState = std::nullopt);

/// The same for a tag the server may have left out, which a head or banner below 1.13 still needs.
void rewrite(std::int32_t nativeType, std::optional<nbt::NbtCompound>& tag, std::int32_t farEnd,
             std::optional<std::int32_t> nativeState);

/// Whether a block state is a head or a banner, whose block entity once carried what the state now says.
[[nodiscard]] bool shapedByState(std::int32_t nativeState);

/// Whether the far end's tag for this block entity needs the state of the block under it.
[[nodiscard]] bool readsState(std::int32_t nativeType, std::int32_t farEnd);

} // namespace basalt::wire::multiversion::java::olderBlockEntities
