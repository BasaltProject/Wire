#pragma once

#include <cstdint>
#include <span>

namespace basalt::wire::multiversion::java {

/// How many kinds of entity data this build knows, which is the width of every row of the table.
inline constexpr int kEntityDataTypeCount = 43;

/// What `version` calls each kind of entity data, indexed by what this build calls it; -1 is a kind
/// that version has never had. Empty for a version the table does not cover.
[[nodiscard]] std::span<const std::int32_t> entityDataTypes(std::int32_t version);

} // namespace basalt::wire::multiversion::java
