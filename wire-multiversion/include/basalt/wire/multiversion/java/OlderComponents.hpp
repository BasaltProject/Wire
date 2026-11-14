#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>

#include <cstdint>
#include <optional>

namespace basalt::wire::multiversion::java::olderComponents {

/// A component as 26.2 and before read it: hidden components renumbered, pot sides as plain item ids.
[[nodiscard]] std::optional<protocol::play::ItemComponent>
template26_2(const protocol::play::ItemComponent& component, const ComposedTables& tables, std::int32_t farEnd);

} // namespace basalt::wire::multiversion::java::olderComponents
