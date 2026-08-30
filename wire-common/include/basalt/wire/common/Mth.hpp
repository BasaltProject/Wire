#pragma once

#include <basalt/wire/common/JavaCast.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

// parity: net.minecraft.util.Mth, the subset noise generation needs.
namespace basalt::wire::common::mth {

inline constexpr double kSinScale = 10430.378350470453;

/// parity: Mth's 65536-entry float sine table. World carvers read these values instead of calling
/// the platform trigonometric functions, and a one-bit trajectory difference moves many blocks.
[[nodiscard]] const float* sineTable() noexcept;

[[nodiscard]] inline int floor(double v) noexcept { return toIntLikeJava(std::floor(v)); }

[[nodiscard]] inline std::int64_t lfloor(double v) noexcept { return toLongLikeJava(std::floor(v)); }

[[nodiscard]] inline float sin(double value) noexcept {
    return sineTable()[static_cast<std::uint32_t>(truncateToInt(toLongLikeJava(value * kSinScale))) & 65535U];
}

[[nodiscard]] inline float cos(double value) noexcept {
    return sineTable()[static_cast<std::uint32_t>(truncateToInt(toLongLikeJava(value * kSinScale + 16384.0))) & 65535U];
}

[[nodiscard]] inline float sin(float value) noexcept { return sin(static_cast<double>(value)); }

[[nodiscard]] inline float cos(float value) noexcept { return cos(static_cast<double>(value)); }

[[nodiscard]] constexpr double lerp(double alpha, double p0, double p1) noexcept { return p0 + alpha * (p1 - p0); }

[[nodiscard]] constexpr float lerp(float alpha, float p0, float p1) noexcept { return p0 + alpha * (p1 - p0); }

[[nodiscard]] constexpr double lerp2(double alpha1, double alpha2, double x00, double x10, double x01,
                                     double x11) noexcept {
    return lerp(alpha2, lerp(alpha1, x00, x10), lerp(alpha1, x01, x11));
}

[[nodiscard]] constexpr double lerp3(double alpha1, double alpha2, double alpha3, double x000, double x100, double x010,
                                     double x110, double x001, double x101, double x011, double x111) noexcept {
    return lerp(alpha3, lerp2(alpha1, alpha2, x000, x100, x010, x110), lerp2(alpha1, alpha2, x001, x101, x011, x111));
}

[[nodiscard]] constexpr double smoothstep(double x) noexcept { return x * x * x * (x * (x * 6.0 - 15.0) + 10.0); }

[[nodiscard]] constexpr double smoothstepDerivative(double x) noexcept { return 30.0 * x * x * (x - 1.0) * (x - 1.0); }

[[nodiscard]] constexpr std::int64_t square(std::int64_t v) noexcept { return v * v; }

[[nodiscard]] constexpr double clamp(double value, double min, double max) noexcept {
    return value < min ? min : std::min(value, max);
}

[[nodiscard]] constexpr float clamp(float value, float min, float max) noexcept {
    return value < min ? min : std::min(value, max);
}

[[nodiscard]] constexpr double inverseLerp(double value, double min, double max) noexcept {
    return (value - min) / (max - min);
}

[[nodiscard]] constexpr double clampedLerp(double factor, double min, double max) noexcept {
    if (factor < 0.0) {
        return min;
    }
    if (factor > 1.0) {
        return max;
    }
    return lerp(factor, min, max);
}

[[nodiscard]] constexpr double clampedMap(double value, double fromMin, double fromMax, double toMin,
                                          double toMax) noexcept {
    return clampedLerp(inverseLerp(value, fromMin, fromMax), toMin, toMax);
}

/// Unclamped linear remap of `value` from `[fromMin, fromMax]` to `[toMin, toMax]`.
[[nodiscard]] constexpr double map(double value, double fromMin, double fromMax, double toMin, double toMax) noexcept {
    return lerp(inverseLerp(value, fromMin, fromMax), toMin, toMax);
}

/// parity: Mth.fastInvSqrt(double), including its single Newton iteration.
[[nodiscard]] inline double fastInvSqrt(double value) noexcept {
    const double half = 0.5 * value;
    const auto bits = static_cast<std::int64_t>(6910469410427058090LL - (std::bit_cast<std::int64_t>(value) >> 1));
    const double estimate = std::bit_cast<double>(bits);
    return estimate * (1.5 - half * estimate * estimate);
}

/// The first index in `[from, to)` for which `condition` holds, assuming it holds for a contiguous
/// suffix of the range; `to` if it never does.
template <typename Condition>
[[nodiscard]] constexpr int binarySearch(int from, int to, Condition condition) {
    int lo = from;
    int len = to - from;
    while (len > 0) {
        const int half = len / 2;
        const int middle = lo + half;
        if (condition(middle)) {
            len = half;
        } else {
            lo = middle + 1;
            len -= half + 1;
        }
    }
    return lo;
}

} // namespace basalt::wire::common::mth
