#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::wire::multiversion::java {

/// A float a server sent as an integer: no value, however absurd, is undefined, and NaN is zero.
[[nodiscard]] inline std::int64_t safeInt(double value) noexcept {
    constexpr double kLimit = 9.0e18;
    return std::isnan(value) ? 0 : static_cast<std::int64_t>(std::clamp(value, -kLimit, kLimit));
}

} // namespace basalt::wire::multiversion::java
