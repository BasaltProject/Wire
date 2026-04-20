#include <basalt/wire/common/timings/TickDurations.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace basalt::wire::common {
namespace {

constexpr double kMaxTicksPerSecond = 20.0;
constexpr std::int64_t kNanosPerTick = 50'000'000;

double toMillis(std::int64_t nanos) noexcept { return static_cast<double>(nanos) / 1'000'000.0; }

/// Nearest-rank: the value an actual tick had, never interpolated.
std::int64_t percentile(const std::vector<std::int64_t>& sorted, double fraction) noexcept {
    const auto rank =
        static_cast<std::size_t>(std::clamp(static_cast<std::ptrdiff_t>(fraction * static_cast<double>(sorted.size())),
                                            std::ptrdiff_t{0}, static_cast<std::ptrdiff_t>(sorted.size()) - 1));
    return sorted[rank];
}

} // namespace

void TickDurations::record(std::int64_t atNanos, std::int64_t durationNanos) {
    const std::uint64_t written = written_.load(std::memory_order_relaxed);
    const std::size_t index = static_cast<std::size_t>(written % durations_.size());
    durations_[index] = durationNanos;
    timestamps_[index] = atNanos;
    written_.store(written + 1, std::memory_order_release);
}

double TickDurations::ticksPerSecond(std::int64_t nowNanos, int windowSeconds) const {
    const std::int64_t cutoff = nowNanos - static_cast<std::int64_t>(windowSeconds) * kNanosPerSecond;
    std::size_t counted = 0;
    std::int64_t oldest = std::numeric_limits<std::int64_t>::max();
    forEachSampleAt(cutoff, [&counted, &oldest](std::int64_t at, std::int64_t) {
        ++counted;
        oldest = std::min(oldest, at);
    });

    if (counted == 0) {
        return 0.0;
    }
    const std::int64_t observed = std::max(nowNanos - oldest, kNanosPerTick);
    const double seconds = static_cast<double>(observed) / static_cast<double>(kNanosPerSecond);
    return std::min(static_cast<double>(counted) / seconds, kMaxTicksPerSecond);
}

DurationSummary TickDurations::summary(std::int64_t nowNanos, int windowSeconds) const {
    const std::int64_t cutoff = nowNanos - static_cast<std::int64_t>(windowSeconds) * kNanosPerSecond;
    std::vector<std::int64_t> collected;
    forEachSampleAt(cutoff, [&collected](std::int64_t, std::int64_t duration) { collected.push_back(duration); });
    if (collected.empty()) {
        return DurationSummary{};
    }

    std::ranges::sort(collected);
    std::int64_t total = 0;
    for (const std::int64_t sample : collected) {
        total += sample;
    }

    return DurationSummary{collected.size(),
                           toMillis(collected.front()),
                           toMillis(percentile(collected, 0.50)),
                           toMillis(percentile(collected, 0.95)),
                           toMillis(collected.back()),
                           toMillis(total / static_cast<std::int64_t>(collected.size()))};
}

void TickDurations::reset() {
    written_.store(0, std::memory_order_relaxed);
    std::ranges::fill(durations_, 0);
    std::ranges::fill(timestamps_, 0);
}

} // namespace basalt::wire::common
