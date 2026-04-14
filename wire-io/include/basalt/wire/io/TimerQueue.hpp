#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace basalt::wire::io {

struct TimerReady {
    std::uint64_t id;
    std::uint64_t token;
};

/// Bounded monotonic deadlines, owned by one I/O loop. reschedule reuses a registration; canceled
/// timers are removed immediately rather than retained as tombstones until their deadline.
class TimerQueue {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    explicit TimerQueue(std::size_t capacity);
    ~TimerQueue();
    TimerQueue(const TimerQueue&) = delete;
    TimerQueue& operator=(const TimerQueue&) = delete;

    [[nodiscard]] std::optional<std::uint64_t> schedule(TimePoint deadline, std::uint64_t token);
    [[nodiscard]] bool reschedule(std::uint64_t id, TimePoint deadline);
    [[nodiscard]] bool cancel(std::uint64_t id);
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::optional<int> nextTimeout(TimePoint now) const;
    /// Returned view expires at the next expire call. Contains no canceled registrations.
    [[nodiscard]] std::span<const TimerReady> expire(TimePoint now);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace basalt::wire::io
