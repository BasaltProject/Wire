#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

namespace basalt::wire::common {

/// min, median, 95th and max of a set of samples, in milliseconds.
struct DurationSummary {
    std::size_t samples = 0;
    double minMillis = 0.0;
    double medianMillis = 0.0;
    double percentile95Millis = 0.0;
    double maxMillis = 0.0;
    double meanMillis = 0.0;

    friend bool operator==(const DurationSummary&, const DurationSummary&) = default;
};

/// How long each tick took, for the last few minutes. TPS hides this: a server pinned at 20 with one
/// tick in fifty taking 80 ms stutters. Hence min, median, 95th and max rather than an average.
///
/// A fixed-size ring with no lock, written by the tick thread: a reader catching a sample mid-write
/// gets one wrong figure in thousands, and locking every tick would be measuring the measurement.
class TickDurations {
public:
    static constexpr std::int64_t kNanosPerSecond = 1'000'000'000;
    /// Five minutes at twenty a second, the longest window anything asks for.
    static constexpr std::size_t kDefaultCapacity = 20 * 60 * 5;

    explicit TickDurations(std::size_t capacity = kDefaultCapacity)
        : durations_(capacity, 0), timestamps_(capacity, 0) {}

    void record(std::int64_t atNanos, std::int64_t durationNanos);

    /// Ticks counted in the window, divided by how much of that window actually exists.
    ///
    /// By observed time, not by the window: a server thirty seconds old has no last five minutes,
    /// and dividing its ticks by three hundred reported 0.14 TPS on a server running perfectly.
    [[nodiscard]] double ticksPerSecond(std::int64_t nowNanos, int windowSeconds) const;

    [[nodiscard]] DurationSummary summary(std::int64_t nowNanos, int windowSeconds) const;

    void reset();

private:
    template <typename Visitor>
    void forEachSampleAt(std::int64_t cutoffNanos, Visitor&& visit) const {
        const std::uint64_t total = written_.load(std::memory_order_relaxed);
        const std::uint64_t available = std::min<std::uint64_t>(total, durations_.size());
        for (std::uint64_t offset = 0; offset < available; ++offset) {
            const std::size_t index = static_cast<std::size_t>((total - 1 - offset) % durations_.size());
            if (timestamps_[index] < cutoffNanos) {
                continue;
            }
            visit(timestamps_[index], durations_[index]);
        }
    }

    std::vector<std::int64_t> durations_;
    std::vector<std::int64_t> timestamps_;
    std::atomic<std::uint64_t> written_{0};
};

} // namespace basalt::wire::common
