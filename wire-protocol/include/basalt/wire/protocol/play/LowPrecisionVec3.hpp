#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <array>

namespace basalt::wire::protocol::play::lowPrecisionVec3 {

/// parity: LpVec3.
inline constexpr double kAbsMaxValue = 1.7179869183E10;
inline constexpr double kAbsMinValue = 3.051944088384301E-5;

[[nodiscard]] std::array<double, 3> read(io::ByteBuffer& buffer);

void write(io::ByteBuffer& buffer, double x, double y, double z);

} // namespace basalt::wire::protocol::play::lowPrecisionVec3
