#pragma once

#include <cstdint>
#include <span>

namespace basalt::wire::multiversion::java {

/// A field one version dropped. `shift` closes the gap it leaves; without it the numbers above it
/// stay where they are.
struct EntityDataIndexRule {
    std::int32_t index;
    bool shift;
    std::span<const std::int32_t> types;
};

/// Where one field went, for the link that says what it keeps rather than what it lost.
struct EntityDataIndexMove {
    std::int32_t from;
    std::int32_t to;
    std::span<const std::int32_t> types;
};

/// What `version` dropped, in the order the rules are applied. Empty for a version that dropped
/// nothing.
[[nodiscard]] std::span<const EntityDataIndexRule> entityDataIndexRules(std::int32_t version);

/// Where `version` keeps each field it has. Empty for a version that lists its losses instead, and
/// an entry with no move there is a field that version keeps no room for.
[[nodiscard]] std::span<const EntityDataIndexMove> entityDataIndexMoves(std::int32_t version);

} // namespace basalt::wire::multiversion::java
