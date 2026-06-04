#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::wire::io {

class ConnectionGate {
public:
    using Clock = std::chrono::steady_clock;

    class Slot {
    public:
        Slot() = default;

        Slot(ConnectionGate* gate, std::string address) : gate_(gate), address_(std::move(address)) {}

        Slot(const Slot&) = delete;
        Slot& operator=(const Slot&) = delete;

        Slot(Slot&& other) noexcept : gate_(std::exchange(other.gate_, nullptr)), address_(std::move(other.address_)) {}

        Slot& operator=(Slot&& other) noexcept;
        ~Slot();

    private:
        ConnectionGate* gate_ = nullptr;
        std::string address_;
    };

    /// Counts one more open connection from `address`; the slot frees it. Refused when `maxPerAddress` is reached.
    [[nodiscard]] std::optional<Slot> open(std::string_view address, std::int32_t maxPerAddress);

    /// True when the address logged in less than `throttleMs` ago; loopback is never throttled.
    [[nodiscard]] bool throttleLogin(std::string_view address, std::int32_t throttleMs, Clock::time_point now);

    /// Empty when the player may finish joining now; otherwise when to ask again. Fifty-millisecond windows.
    [[nodiscard]] std::optional<Clock::time_point> joinWait(std::int32_t perTick, Clock::time_point now);

private:
    void release(const std::string& address);

    std::mutex mutex_;
    Clock::time_point joinWindow_{};
    std::int32_t joinsInWindow_ = 0;
    std::map<std::string, std::int32_t, std::less<>> open_;
    std::map<std::string, Clock::time_point, std::less<>> lastLogin_;
    Clock::time_point lastPrune_{};
};

} // namespace basalt::wire::io
