#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <array>

namespace basalt::wire::protocol::play::lowPrecisionVec3 {

/// parity: LpVec3, the quantised vector 26.2 introduced for the places a rough direction is enough.
///
/// Six bytes carry three fifteen-bit fractions and a scale of up to four; anything bigger sets a
/// continuation bit and writes the rest of the scale as a varint. A vector shorter than the smallest
/// step is one zero byte, which is why the common "nothing" case costs a single byte.
inline constexpr double kAbsMaxValue = 1.7179869183E10;
inline constexpr double kAbsMinValue = 3.051944088384301E-5;

[[nodiscard]] std::array<double, 3> read(io::ByteBuffer& buffer);

void write(io::ByteBuffer& buffer, double x, double y, double z);

} // namespace basalt::wire::protocol::play::lowPrecisionVec3
