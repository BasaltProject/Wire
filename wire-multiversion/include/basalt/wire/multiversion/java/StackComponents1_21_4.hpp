#pragma once

#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::wire::multiversion::java::stackComponents1_21_4 {

using Nested =
    std::function<std::optional<protocol::play::WireItemStack>(const std::optional<protocol::play::WireItemStack>&)>;

/// Gives each component of a stack the payload 1.20.5 to 1.21.4 read, still under this server's ids.
/// Drops what that era cannot read; `nested` turns a stack held inside another into the far end's.
/// Returns whether the tooltip is hidden, which those versions said with a component of their own.
[[nodiscard]] bool rewrite(protocol::play::WireItemStack& item, std::int32_t farEnd, const Nested& nested);

/// From 1.21.5 to 1.21.11 a stack held in a component is a whole stack, count first, not an item template.
void nestedStacks(protocol::play::WireItemStack& item, std::int32_t farEnd, const Nested& nested);

/// The id of 1.20.5 to 1.21.4's hide_tooltip component in the far end's numbering.
[[nodiscard]] std::int32_t hideTooltipId(std::int32_t farEnd) noexcept;

} // namespace basalt::wire::multiversion::java::stackComponents1_21_4
