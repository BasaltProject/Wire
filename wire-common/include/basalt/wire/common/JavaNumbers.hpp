#pragma once

#include <string>

namespace basalt::wire::common {

/// parity: Double.toString, shortest round-trip digits, scientific notation outside [1e-3, 1e7).
[[nodiscard]] std::string javaToString(double value);

/// parity: `Float.toString`, the same rules over the float's own shortest digits.
[[nodiscard]] std::string javaToString(float value);

} // namespace basalt::wire::common
