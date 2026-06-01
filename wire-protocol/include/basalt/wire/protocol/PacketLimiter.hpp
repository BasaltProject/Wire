#pragma once

#include <basalt/wire/protocol/PacketNames.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace basalt::wire::protocol {

struct PacketRule {
    packetNames::Phase phase = packetNames::Phase::kPlay;
    std::int32_t id = 0;
    std::int32_t maxPerSecond = 0;
    bool kick = false;
};

struct PacketLimits {
    std::int32_t rateLimit = 0;
    std::vector<PacketRule> rules;
};

/// One client's packet budget: a vanilla-style average rate cap plus per-packet caps per second.
class PacketLimiter {
public:
    enum class Verdict : std::uint8_t { kAllow, kDrop, kKick };

    using Clock = std::chrono::steady_clock;

    PacketLimiter() = default;
    explicit PacketLimiter(std::shared_ptr<const PacketLimits> limits);

    [[nodiscard]] bool active() const noexcept;

    [[nodiscard]] Verdict receive(packetNames::Phase phase, std::int32_t id, Clock::time_point now);

    /// parity: `Connection.getAverageReceivedPackets`.
    [[nodiscard]] float average() const noexcept { return average_; }

private:
    void roll(Clock::time_point now);

    std::shared_ptr<const PacketLimits> limits_;
    std::vector<std::int32_t> ruleCounts_;
    Clock::time_point windowStart_{};
    bool started_ = false;
    std::int32_t received_ = 0;
    float average_ = 0.0F;
};

/// parity: `TickThrottler`, decayed by elapsed time for callers that have no tick.
class SpamThrottle {
public:
    using Clock = std::chrono::steady_clock;

    static constexpr std::int32_t kStep = 20;

    void configure(std::int32_t thresholdSeconds) noexcept { threshold_ = kStep * thresholdSeconds; }

    /// Counts one message and says whether the sender is still under the threshold.
    [[nodiscard]] bool increment(Clock::time_point now) noexcept;

private:
    std::int32_t threshold_ = 0;
    std::int32_t count_ = 0;
    bool started_ = false;
    Clock::time_point last_{};
};

} // namespace basalt::wire::protocol
