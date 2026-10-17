#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::wire::protocol::play {

/// The path of an entity type in the target version's registry, without `minecraft:`; empty for an id it lacks.
[[nodiscard]] std::string_view javaEntityTypeName(std::int32_t protocolId);

} // namespace basalt::wire::protocol::play
