#pragma once

#include <cstdint>
#include <span>

namespace basalt::wire::multiversion::java {

[[nodiscard]] std::span<const std::span<const std::uint8_t>> registriesAddedIn1_21_11();

[[nodiscard]] std::span<const std::uint8_t> dimensionTypesOf1_21_11();

} // namespace basalt::wire::multiversion::java
