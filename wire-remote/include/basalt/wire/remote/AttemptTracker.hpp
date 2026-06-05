#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace basalt::wire::remote {

/// Per-address lockout with exponential backoff, plus a fixed-window request counter.
class AttemptTracker {
public:
    using Clock = std::chrono::steady_clock;

    AttemptTracker(std::int32_t maxFailures, std::int32_t lockoutSeconds)
        : maxFailures_(maxFailures), lockoutSeconds_(lockoutSeconds) {}

    /// Whole seconds left of a lockout, zero when the address may try.
    [[nodiscard]] std::int32_t lockedFor(const std::string& address, Clock::time_point now) const;

    void recordFailure(const std::string& address, Clock::time_point now);

    void recordSuccess(const std::string& address);

    /// Counts one request; false once the address went over `perMinute` inside the current minute.
    [[nodiscard]] bool allowRequest(const std::string& address, Clock::time_point now, std::int32_t perMinute);

    void sweep(Clock::time_point now);

private:
    struct Entry {
        std::int32_t failures = 0;
        std::int32_t lockouts = 0;
        Clock::time_point lockedUntil{};
        Clock::time_point lastSeen{};
        Clock::time_point windowStart{};
        std::int32_t windowCount = 0;
    };

    std::int32_t maxFailures_;
    std::int32_t lockoutSeconds_;
    std::unordered_map<std::string, Entry> entries_;
};

} // namespace basalt::wire::remote
