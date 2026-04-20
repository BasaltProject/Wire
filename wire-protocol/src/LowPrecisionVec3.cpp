#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/LowPrecisionVec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::wire::protocol::play::lowPrecisionVec3 {
namespace {

constexpr std::uint64_t kDataMask = 32767;
constexpr double kMaxQuantized = 32766.0;
constexpr std::uint64_t kScaleMask = 3;
constexpr std::uint64_t kContinuation = 4;
constexpr int kXOffset = 3;
constexpr int kYOffset = 18;
constexpr int kZOffset = 33;

double sanitize(double value) noexcept {
    return std::isnan(value) ? 0.0 : std::clamp(value, -kAbsMaxValue, kAbsMaxValue);
}

std::uint64_t quantize(double value) noexcept {
    return static_cast<std::uint64_t>(std::llround((value * 0.5 + 0.5) * kMaxQuantized));
}

double dequantize(std::uint64_t value) noexcept {
    return std::min(static_cast<double>(value & kDataMask), kMaxQuantized) * 2.0 / kMaxQuantized - 1.0;
}

} // namespace

std::array<double, 3> read(io::ByteBuffer& buffer) {
    const std::uint64_t lowest = buffer.readByte();
    if (buffer.failed() || lowest == 0) {
        return {0.0, 0.0, 0.0};
    }
    const std::uint64_t middle = buffer.readByte();
    const auto highest = static_cast<std::uint64_t>(static_cast<std::uint32_t>(buffer.readInt()));
    const std::uint64_t packed = (highest << 16) | (middle << 8) | lowest;

    std::uint64_t scale = lowest & kScaleMask;
    if ((lowest & kContinuation) == kContinuation) {
        scale |= static_cast<std::uint64_t>(static_cast<std::uint32_t>(varInt::read(buffer))) << 2;
    }
    const auto factor = static_cast<double>(scale);
    return {dequantize(packed >> kXOffset) * factor, dequantize(packed >> kYOffset) * factor,
            dequantize(packed >> kZOffset) * factor};
}

void write(io::ByteBuffer& buffer, double x, double y, double z) {
    const double sx = sanitize(x);
    const double sy = sanitize(y);
    const double sz = sanitize(z);
    const double longest = std::max({std::abs(sx), std::abs(sy), std::abs(sz)});
    if (longest < kAbsMinValue) {
        buffer.writeByte(0);
        return;
    }

    const auto scale = static_cast<std::uint64_t>(std::ceil(longest));
    const bool partial = (scale & kScaleMask) != scale;
    const std::uint64_t markers = partial ? ((scale & kScaleMask) | kContinuation) : scale;
    const auto divisor = static_cast<double>(scale);
    const std::uint64_t packed = markers | (quantize(sx / divisor) << kXOffset) | (quantize(sy / divisor) << kYOffset) |
                                 (quantize(sz / divisor) << kZOffset);

    buffer.writeByte(static_cast<std::uint8_t>(packed));
    buffer.writeByte(static_cast<std::uint8_t>(packed >> 8));
    buffer.writeInt(static_cast<std::int32_t>(static_cast<std::uint32_t>(packed >> 16)));
    if (partial) {
        varInt::write(buffer, static_cast<std::int32_t>(scale >> 2));
    }
}

} // namespace basalt::wire::protocol::play::lowPrecisionVec3
