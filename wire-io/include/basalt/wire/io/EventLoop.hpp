#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace basalt::wire::io {

struct IoInterest {
    bool readable = true;
    bool writable = false;
};

struct IoReady {
    std::int64_t handle = -1;
    std::uint64_t token = 0;
    bool readable = false;
    bool writable = false;
    bool closed = false;
};

/// Registration and wait belong to one owner; wake may be called from another thread.
class EventLoop {
public:
    EventLoop();
    ~EventLoop();
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    [[nodiscard]] std::expected<void, int> watch(std::int64_t handle, IoInterest interest, std::uint64_t token);
    void unwatch(std::int64_t handle);
    [[nodiscard]] std::size_t watchedCount() const noexcept;

    [[nodiscard]] std::expected<std::span<const IoReady>, int> wait(std::optional<int> timeoutMillis = std::nullopt);

    /// Coalesces notifications; the caller owns the task queue and must drain it after every wait.
    [[nodiscard]] std::expected<void, int> wake() const;

    void watch(std::int64_t handle);
    [[nodiscard]] std::expected<std::vector<std::int64_t>, int>
    waitReadable(std::optional<int> timeoutMillis = std::nullopt);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace basalt::wire::io
