#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace basalt::wire::common {

/// Java's `d2i`/`d2l`: NaN becomes 0 and out-of-range saturates to the extreme. A plain
/// `static_cast` is undefined behaviour there, so every float-to-integer narrowing that has to
/// stay bit-exact with vanilla goes through these.
template <typename T>
[[nodiscard]] constexpr T narrowLikeJava(double value) noexcept {
    if (std::isnan(value)) {
        return 0;
    }
    constexpr auto lowest = static_cast<double>(std::numeric_limits<T>::min());
    constexpr auto highest = static_cast<double>(std::numeric_limits<T>::max());
    if (value <= lowest) {
        return std::numeric_limits<T>::min();
    }
    if (value >= highest) {
        return std::numeric_limits<T>::max();
    }
    return static_cast<T>(value);
}

[[nodiscard]] constexpr std::int32_t toIntLikeJava(double value) noexcept {
    return narrowLikeJava<std::int32_t>(value);
}

[[nodiscard]] constexpr std::int64_t toLongLikeJava(double value) noexcept {
    return narrowLikeJava<std::int64_t>(value);
}

/// Java's `l2i`: keep the low 32 bits. Well defined in C++ only through the unsigned round trip.
[[nodiscard]] constexpr std::int32_t truncateToInt(std::int64_t value) noexcept {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(value)));
}

} // namespace basalt::wire::common
