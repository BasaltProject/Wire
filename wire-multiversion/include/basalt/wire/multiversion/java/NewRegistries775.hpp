#pragma once

#include <cstdint>
#include <span>

namespace basalt::wire::multiversion::java {

[[nodiscard]] std::span<const std::span<const std::uint8_t>> registriesAddedIn26_1();

} // namespace basalt::wire::multiversion::java
