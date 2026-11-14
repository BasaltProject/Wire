#pragma once

#include <cstdint>
#include <span>

namespace basalt::wire::multiversion::java {

struct EntityDataIndexRule {
    std::int32_t index;
    bool shift;
    std::span<const std::int32_t> types;
};

struct EntityDataIndexMove {
    std::int32_t from;
    std::int32_t to;
    std::span<const std::int32_t> types;
    /// The far end's kind for it when the value's own kind would be wrong there; -1 keeps the value's.
    std::int32_t as = -1;
};

/// A far-end byte that holds -1 for true, the way 1.8 tells a child apart.
inline constexpr std::int32_t kEntityDataChildByte = 0x100;

/// What `version` dropped, in the order the rules are applied.
[[nodiscard]] std::span<const EntityDataIndexRule> entityDataIndexRules(std::int32_t version);

[[nodiscard]] std::span<const EntityDataIndexMove> entityDataIndexMoves(std::int32_t version);

/// Fields `version` kept but placed in another order; any other index of the type stays where it is.
[[nodiscard]] std::span<const EntityDataIndexMove> entityDataIndexSwaps(std::int32_t version);

} // namespace basalt::wire::multiversion::java
