#include <basalt/wire/io/ConnectionGate.hpp>

namespace basalt::wire::io {
namespace {
constexpr std::chrono::milliseconds kJoinWindow{50};
constexpr std::size_t kMaxTrackedLogins = 65536;

bool loopback(std::string_view address) {
    return address.starts_with("127.") || address.starts_with("::ffff:127.") || address == "::1" ||
           address == "0:0:0:0:0:0:0:1";
}
} // namespace

ConnectionGate::Slot& ConnectionGate::Slot::operator=(Slot&& other) noexcept {
    if (this != &other) {
        if (gate_ != nullptr)
            gate_->release(address_);
        gate_ = std::exchange(other.gate_, nullptr);
        address_ = std::move(other.address_);
    }
    return *this;
}

ConnectionGate::Slot::~Slot() {
    if (gate_ != nullptr)
        gate_->release(address_);
}

std::optional<ConnectionGate::Slot> ConnectionGate::open(std::string_view address, std::int32_t maxPerAddress) {
    const std::lock_guard lock{mutex_};
    auto found = open_.find(address);
    if (found == open_.end())
        found = open_.emplace(std::string{address}, 0).first;
    if (maxPerAddress > 0 && found->second >= maxPerAddress) {
        if (found->second == 0)
            open_.erase(found);
        return std::nullopt;
    }
    ++found->second;
    return Slot{this, std::string{address}};
}

void ConnectionGate::release(const std::string& address) {
    const std::lock_guard lock{mutex_};
    const auto found = open_.find(address);
    if (found != open_.end() && --found->second <= 0)
        open_.erase(found);
}

// parity: Paper `max-joins-per-tick`, a tick being fifty milliseconds here.
std::optional<ConnectionGate::Clock::time_point> ConnectionGate::joinWait(std::int32_t perTick, Clock::time_point now) {
    if (perTick <= 0)
        return std::nullopt;
    const std::lock_guard lock{mutex_};
    if (now - joinWindow_ >= kJoinWindow) {
        joinWindow_ = now;
        joinsInWindow_ = 0;
    }
    if (joinsInWindow_ < perTick) {
        ++joinsInWindow_;
        return std::nullopt;
    }
    return joinWindow_ + kJoinWindow;
}

// parity: CraftBukkit `connection-throttle`, which refreshes the stamp even when it refuses.
bool ConnectionGate::throttleLogin(std::string_view address, std::int32_t throttleMs, Clock::time_point now) {
    if (throttleMs <= 0 || loopback(address))
        return false;
    const std::lock_guard lock{mutex_};
    if (now - lastPrune_ >= std::chrono::minutes(1)) {
        lastPrune_ = now;
        std::erase_if(lastLogin_, [now](const auto& entry) { return now - entry.second > std::chrono::minutes(10); });
    }
    const auto found = lastLogin_.find(address);
    if (found == lastLogin_.end()) {
        if (lastLogin_.size() < kMaxTrackedLogins)
            lastLogin_.emplace(std::string{address}, now);
        return false;
    }
    const bool throttled = now - found->second < std::chrono::milliseconds(throttleMs);
    found->second = now;
    return throttled;
}

} // namespace basalt::wire::io
