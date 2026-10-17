#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::wire::plugins {

/// The order a plugin's handlers for one event run in, FIRST to LAST, the same words in Server and in Proxy.
enum class EventPriority : std::uint8_t { First, Early, Normal, Late, Last };

[[nodiscard]] std::string_view priorityName(EventPriority priority);

/// Case does not matter.
[[nodiscard]] std::optional<EventPriority> priorityOf(std::string_view name);

} // namespace basalt::wire::plugins
