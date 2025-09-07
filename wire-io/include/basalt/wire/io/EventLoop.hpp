#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

namespace basalt::wire::io {

/// Readiness-based event loop over `poll()`, not `select()`.
///
/// `poll` is in POSIX and Winsock (as WSAPoll) alike, so it needs no per-platform branch beyond the
/// header, and unlike `select` it has no FD_SETSIZE ceiling: the old loop was capped at 1024
/// descriptors on POSIX and 64 on Windows, which the proxy would have hit. Descriptors live in one
/// contiguous array that is handed to the kernel as-is.
class EventLoop {
public:
    void watch(std::int64_t handle);

    void unwatch(std::int64_t handle);

    [[nodiscard]] std::size_t watchedCount() const noexcept { return watched_.size(); }

    /// Blocks up to the timeout (indefinitely when absent) for at least one watched handle to become
    /// readable, and returns those that did. The returned span is valid until the next call.
    [[nodiscard]] std::expected<std::vector<std::int64_t>, int>
    waitReadable(std::optional<int> timeoutMillis = std::nullopt);

private:
    struct Watched {
        std::int64_t handle;
    };

    std::vector<std::int64_t> watched_;
    std::vector<std::int64_t> ready_;
    /// Raw pollfd storage, kept as bytes so the header does not leak into every consumer.
    std::vector<std::uint8_t> pollStorage_;
};

} // namespace basalt::wire::io
