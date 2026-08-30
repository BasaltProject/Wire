#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::common {

class Timings;

/// One timer's numbers at a moment in time.
struct TimingSnapshot {
    std::string name;
    std::int64_t calls = 0;
    std::int64_t totalNanos = 0;
    std::int64_t maxNanos = 0;

    [[nodiscard]] std::int64_t averageNanos() const noexcept { return calls == 0 ? 0 : totalNanos / calls; }

    friend bool operator==(const TimingSnapshot&, const TimingSnapshot&) = default;
};

/// One named section of work, measured every time it runs. The counters are atomic because the
/// interesting sections are the ones several threads run at once, and a lost increment would be
/// worse than no count when the whole use is comparing totals.
///
/// The maximum is kept and the minimum is not: the question is what made the server stutter.
class Timing {
public:
    Timing(std::string name, const Timings& owner) : name_(std::move(name)), owner_(&owner) {}

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /// Read before measuring, so a disabled registry costs one atomic read and nothing else.
    [[nodiscard]] bool isEnabled() const noexcept;

    void record(std::int64_t nanos) noexcept;

    [[nodiscard]] TimingSnapshot snapshot() const;

    void reset() noexcept;

    /// Measures the callable and returns what it returned. A disabled timing adds one atomic read
    /// to the call: instrumentation that costs something when switched off gets removed again.
    template <typename Block>
    decltype(auto) time(Block&& block) {
        if (!isEnabled()) {
            return std::forward<Block>(block)();
        }
        const auto started = std::chrono::steady_clock::now();

        struct Recorder {
            Timing& timing;
            std::chrono::steady_clock::time_point started;

            ~Recorder() {
                timing.record(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started)
                        .count());
            }
        } recorder{*this, started};

        return std::forward<Block>(block)();
    }

private:
    std::string name_;
    const Timings* owner_;
    std::atomic<std::int64_t> calls_{0};
    std::atomic<std::int64_t> totalNanos_{0};
    std::atomic<std::int64_t> maxNanos_{0};
};

/// Every named section this server measures. Off by default rather than behind a build option:
/// measuring costs a clock read per section per call, and a server nobody is investigating should
/// not pay it. `/timings` turns it on.
///
/// Names nest with dots (`player.tick`), but nothing enforces that a parent exists: a name is a
/// label, and a hierarchy kept in step with the code by hand is how these rot.
class Timings {
public:
    [[nodiscard]] bool enabled() const noexcept { return enabled_.load(std::memory_order_relaxed); }

    void setEnabled(bool value) noexcept { enabled_.store(value, std::memory_order_relaxed); }

    /// The timer for a name, creating it the first time. The reference stays valid for the life of
    /// the registry, so a hot section looks its timer up once and keeps it.
    [[nodiscard]] Timing& of(std::string_view name);

    [[nodiscard]] std::vector<TimingSnapshot> snapshot() const;

    /// Clears every counter and restarts the window. Called when measuring is switched on.
    void reset(std::int64_t nowNanos);

    [[nodiscard]] std::int64_t windowNanos(std::int64_t nowNanos) const noexcept;

private:
    // A lock, not a lock-free map: creation happens a handful of times at startup and never on the
    // hot path, because callers hold the reference they were given.
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<Timing>> timings_;
    std::atomic<bool> enabled_{false};
    std::atomic<std::int64_t> startedAtNanos_{0};
};

} // namespace basalt::wire::common
