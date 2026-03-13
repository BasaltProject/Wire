#pragma once

#include <string>

namespace basalt::wire::common {

/// parity: `Double.toString`: the shortest digits that read back, with at least one after the
/// point, and scientific notation (`1.0E7`) outside `[1e-3, 1e7)`.
[[nodiscard]] std::string javaToString(double value);

/// parity: `Float.toString`, the same rules over the float's own shortest digits.
[[nodiscard]] std::string javaToString(float value);

} // namespace basalt::wire::common
