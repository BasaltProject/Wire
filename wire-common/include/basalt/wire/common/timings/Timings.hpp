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

struct TimingSnapshot {
    std::string name;
    std::int64_t calls = 0;
    std::int64_t totalNanos = 0;
    std::int64_t maxNanos = 0;

    [[nodiscard]] std::int64_t averageNanos() const noexcept { return calls == 0 ? 0 : totalNanos / calls; }

    friend bool operator==(const TimingSnapshot&, const TimingSnapshot&) = default;
};

/// Counters are atomic because several threads run the same section; only the maximum is kept.
class Timing {
public:
    Timing(std::string name, const Timings& owner) : name_(std::move(name)), owner_(&owner) {}

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    [[nodiscard]] bool isEnabled() const noexcept;

    void record(std::int64_t nanos) noexcept;

    [[nodiscard]] TimingSnapshot snapshot() const;

    void reset() noexcept;

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

class Timings {
public:
    [[nodiscard]] bool enabled() const noexcept { return enabled_.load(std::memory_order_relaxed); }

    void setEnabled(bool value) noexcept { enabled_.store(value, std::memory_order_relaxed); }

    [[nodiscard]] Timing& of(std::string_view name);

    [[nodiscard]] std::vector<TimingSnapshot> snapshot() const;

    void reset(std::int64_t nowNanos);

    [[nodiscard]] std::int64_t windowNanos(std::int64_t nowNanos) const noexcept;

private:
    // A lock, not a lock-free map: creation is rare and never on the hot path.
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<Timing>> timings_;
    std::atomic<bool> enabled_{false};
    std::atomic<std::int64_t> startedAtNanos_{0};
};

} // namespace basalt::wire::common
