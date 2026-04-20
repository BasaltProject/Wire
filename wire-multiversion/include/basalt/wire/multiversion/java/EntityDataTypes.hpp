#pragma once

#include <cstdint>
#include <span>

namespace basalt::wire::multiversion::java {

inline constexpr int kEntityDataTypeCount = 44;

[[nodiscard]] std::span<const std::int32_t> entityDataTypes(std::int32_t version);

} // namespace basalt::wire::multiversion::java
