#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::wire::common {

/// A float a peer sent as an integer: no value, however absurd, is undefined, and NaN is zero.
[[nodiscard]] inline std::int64_t safeInt(double value) noexcept {
    constexpr double kLimit = 9.0e18;
    return std::isnan(value) ? 0 : static_cast<std::int64_t>(std::clamp(value, -kLimit, kLimit));
}

[[nodiscard]] inline std::int32_t safeInt32(double value) noexcept {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(safeInt(value), INT32_MIN, INT32_MAX));
}

[[nodiscard]] inline std::int64_t safeInt(float value) noexcept {
    return safeInt(static_cast<double>(value));
}

[[nodiscard]] inline std::int32_t safeInt32(float value) noexcept {
    return safeInt32(static_cast<double>(value));
}

} // namespace basalt::wire::common
