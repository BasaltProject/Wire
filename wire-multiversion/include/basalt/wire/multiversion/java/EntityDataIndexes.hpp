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
};

/// What `version` dropped, in the order the rules are applied.
[[nodiscard]] std::span<const EntityDataIndexRule> entityDataIndexRules(std::int32_t version);

[[nodiscard]] std::span<const EntityDataIndexMove> entityDataIndexMoves(std::int32_t version);

} // namespace basalt::wire::multiversion::java
