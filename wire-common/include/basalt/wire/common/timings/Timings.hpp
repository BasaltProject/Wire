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

/// One frame of the call tree: a section as it was entered from its parent, not the section in general.
struct ProfileNode {
    std::string name;
    std::int64_t calls = 0;
    std::int64_t totalNanos = 0;
    std::int64_t maxNanos = 0;
    std::vector<ProfileNode> children;

    /// Time spent in the frame itself, without what it called.
    [[nodiscard]] std::int64_t selfNanos() const noexcept;
};

/// What one thread did while measuring, rooted at a synthetic node that has no time of its own.
struct ThreadProfile {
    std::string name;
    ProfileNode root;
};

/// Opaque handle returned by entering a section so leaving it finds the same frame again.
struct ProfileScope {
    void* node = nullptr;
    std::uint64_t generation = 0;
};

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

    /// Counts a duration measured elsewhere, attributing it under whatever section this thread is inside.
    void record(std::int64_t nanos) noexcept;

    [[nodiscard]] TimingSnapshot snapshot() const;

    void reset() noexcept;

    template <typename Block>
    decltype(auto) time(Block&& block) {
        if (!isEnabled()) {
            return std::forward<Block>(block)();
        }
        const ProfileScope scope = enter();
        const auto started = std::chrono::steady_clock::now();

        struct Recorder {
            Timing& timing;
            ProfileScope scope;
            std::chrono::steady_clock::time_point started;

            ~Recorder() {
                timing.leave(scope,
                             std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() -
                                                                                  started)
                                 .count());
            }
        } recorder{*this, scope, started};

        return std::forward<Block>(block)();
    }

private:
    friend class ScopedTiming;

    [[nodiscard]] ProfileScope enter() const noexcept;

    void leave(ProfileScope scope, std::int64_t nanos) noexcept;

    void add(std::int64_t nanos) noexcept;

    std::string name_;
    const Timings* owner_;
    std::atomic<std::int64_t> calls_{0};
    std::atomic<std::int64_t> totalNanos_{0};
    std::atomic<std::int64_t> maxNanos_{0};
};

class Timings {
public:
    Timings();
    ~Timings();
    Timings(const Timings&) = delete;
    Timings& operator=(const Timings&) = delete;

    [[nodiscard]] bool enabled() const noexcept { return enabled_.load(std::memory_order_relaxed); }

    void setEnabled(bool value) noexcept { enabled_.store(value, std::memory_order_relaxed); }

    [[nodiscard]] Timing& of(std::string_view name);

    [[nodiscard]] std::vector<TimingSnapshot> snapshot() const;

    /// Labels the calling thread in `profiles`; unnamed threads are told apart by number.
    void nameThread(std::string_view name) const;

    /// The call tree of every thread that ran a section since the last reset, busiest first.
    [[nodiscard]] std::vector<ThreadProfile> profiles() const;

    void reset(std::int64_t nowNanos);

    [[nodiscard]] std::int64_t windowNanos(std::int64_t nowNanos) const noexcept;

private:
    friend class Timing;

    struct ThreadState;

    [[nodiscard]] ThreadState& threadState() const;

    [[nodiscard]] ProfileScope enterScope(const Timing& timing) const;

    void leaveScope(ProfileScope scope, std::int64_t nanos) const;

    void attribute(const Timing& timing, std::int64_t nanos) const;

    // A lock, not a lock-free map: creation is rare and never on the hot path.
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<Timing>> timings_;
    std::atomic<bool> enabled_{false};
    std::atomic<std::int64_t> startedAtNanos_{0};
    // Bumped by `reset`; a thread discards its tree when it notices, so no thread's frames are freed under it.
    std::atomic<std::uint64_t> generation_{1};
    std::uint64_t instance_;
    mutable std::mutex threadsMutex_;
    mutable std::vector<std::shared_ptr<ThreadState>> threads_;
};

/// Times the rest of the enclosing block, which `continue` and early returns leave without a lambda in the way.
/// Costs one flag read when measuring is off, and builds no name then either.
class ScopedTiming {
public:
    ScopedTiming(Timings& timings, std::string_view name) {
        if (timings.enabled()) {
            begin(timings.of(name));
        }
    }

    ScopedTiming(Timings& timings, std::string_view prefix, std::string_view name) {
        if (timings.enabled()) {
            std::string joined;
            joined.reserve(prefix.size() + name.size());
            joined.append(prefix).append(name);
            begin(timings.of(joined));
        }
    }

    ScopedTiming(const ScopedTiming&) = delete;
    ScopedTiming& operator=(const ScopedTiming&) = delete;

    ~ScopedTiming() {
        if (timing_ != nullptr) {
            timing_->leave(scope_, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                       std::chrono::steady_clock::now() - started_)
                                       .count());
        }
    }

private:
    void begin(Timing& timing) {
        timing_ = &timing;
        scope_ = timing.enter();
        started_ = std::chrono::steady_clock::now();
    }

    Timing* timing_ = nullptr;
    ProfileScope scope_;
    std::chrono::steady_clock::time_point started_;
};

} // namespace basalt::wire::common
