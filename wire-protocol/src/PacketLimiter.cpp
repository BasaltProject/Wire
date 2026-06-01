#include <basalt/wire/protocol/PacketLimiter.hpp>

#include <algorithm>
#include <utility>

namespace basalt::wire::protocol {

PacketLimiter::PacketLimiter(std::shared_ptr<const PacketLimits> limits) : limits_(std::move(limits)) {
    if (limits_ != nullptr)
        ruleCounts_.assign(limits_->rules.size(), 0);
}

bool PacketLimiter::active() const noexcept {
    return limits_ != nullptr && (limits_->rateLimit > 0 || !limits_->rules.empty());
}

// parity: `Connection.tickSecond`, folded in lazily so an idle connection costs nothing.
void PacketLimiter::roll(Clock::time_point now) {
    if (!started_) {
        started_ = true;
        windowStart_ = now;
        return;
    }
    while (now - windowStart_ >= std::chrono::seconds{1}) {
        average_ = (0.25F * static_cast<float>(received_)) + (0.75F * average_);
        received_ = 0;
        std::ranges::fill(ruleCounts_, 0);
        windowStart_ += std::chrono::seconds{1};
        if (limits_->rateLimit > 0 && average_ > static_cast<float>(limits_->rateLimit))
            return;
    }
}

// parity: `RateKickingConnection.tickSecond`.
PacketLimiter::Verdict PacketLimiter::receive(packetNames::Phase phase, std::int32_t id, Clock::time_point now) {
    roll(now);
    if (limits_->rateLimit > 0 && average_ > static_cast<float>(limits_->rateLimit))
        return Verdict::kKick;
    ++received_;
    for (std::size_t i = 0; i < limits_->rules.size(); ++i) {
        const PacketRule& rule = limits_->rules[i];
        if (rule.phase != phase || rule.id != id)
            continue;
        if (++ruleCounts_[i] > rule.maxPerSecond)
            return rule.kick ? Verdict::kKick : Verdict::kDrop;
    }
    return Verdict::kAllow;
}

bool SpamThrottle::increment(Clock::time_point now) noexcept {
    if (threshold_ <= 0)
        return true;
    if (started_) {
        const auto ticks = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_).count() / 50;
        count_ = ticks >= count_ ? 0 : count_ - static_cast<std::int32_t>(ticks);
        last_ += std::chrono::milliseconds{ticks * 50};
    } else {
        started_ = true;
        last_ = now;
    }
    count_ += kStep;
    return count_ < threshold_;
}

} // namespace basalt::wire::protocol
