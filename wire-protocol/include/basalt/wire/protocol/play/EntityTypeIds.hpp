#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::wire::protocol::play {

/// The path of an entity type in the target version's registry, without `minecraft:`; empty for an id it lacks.
[[nodiscard]] std::string_view javaEntityTypeName(std::int32_t protocolId);

struct EntityDimensions {
    float width = 0.0F;
    float height = 0.0F;
};

/// The hit box of an entity type by its path, in blocks; nothing for a name the target version lacks.
[[nodiscard]] std::optional<EntityDimensions> javaEntityDimensions(std::string_view path);

} // namespace basalt::wire::protocol::play
