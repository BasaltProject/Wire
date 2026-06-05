#include <basalt/wire/remote/AttemptTracker.hpp>

#include <algorithm>

namespace basalt::wire::remote {
namespace {

constexpr std::int64_t kMaxLockoutSeconds = 24 * 60 * 60;
constexpr std::chrono::minutes kForgetAfter{60};

} // namespace

std::int32_t AttemptTracker::lockedFor(const std::string& address, Clock::time_point now) const {
    const auto found = entries_.find(address);
    if (found == entries_.end() || found->second.lockedUntil <= now) {
        return 0;
    }
    const auto left = std::chrono::duration_cast<std::chrono::seconds>(found->second.lockedUntil - now).count();
    return static_cast<std::int32_t>(left + 1);
}

void AttemptTracker::recordFailure(const std::string& address, Clock::time_point now) {
    Entry& entry = entries_[address];
    entry.lastSeen = now;
    if (++entry.failures < maxFailures_) {
        return;
    }
    entry.failures = 0;
    const std::int32_t doublings = std::min(entry.lockouts, 20);
    ++entry.lockouts;
    const std::int64_t seconds =
        std::min<std::int64_t>(static_cast<std::int64_t>(lockoutSeconds_) << doublings, kMaxLockoutSeconds);
    entry.lockedUntil = now + std::chrono::seconds{seconds};
}

void AttemptTracker::recordSuccess(const std::string& address) {
    const auto found = entries_.find(address);
    if (found != entries_.end()) {
        found->second.failures = 0;
        found->second.lockouts = 0;
    }
}

bool AttemptTracker::allowRequest(const std::string& address, Clock::time_point now, std::int32_t perMinute) {
    Entry& entry = entries_[address];
    entry.lastSeen = now;
    if (now - entry.windowStart >= std::chrono::minutes{1}) {
        entry.windowStart = now;
        entry.windowCount = 0;
    }
    return ++entry.windowCount <= perMinute;
}

void AttemptTracker::sweep(Clock::time_point now) {
    std::erase_if(entries_, [now](const auto& item) {
        return item.second.lockedUntil <= now && now - item.second.lastSeen >= kForgetAfter;
    });
}

} // namespace basalt::wire::remote
