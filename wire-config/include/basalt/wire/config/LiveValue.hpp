#pragma once

#include <memory>
#include <mutex>
#include <utility>

namespace basalt::wire::config {

/// A configuration snapshot readers share and a reload swaps whole, so a half-read file is never visible.
template <typename T>
class LiveValue {
public:
    explicit LiveValue(T initial) : snapshot_(std::make_shared<const T>(std::move(initial))) {}

    [[nodiscard]] std::shared_ptr<const T> current() const {
        const std::lock_guard<std::mutex> lock{mutex_};
        return snapshot_;
    }

    void replace(T next) {
        std::shared_ptr<const T> replacement = std::make_shared<const T>(std::move(next));
        const std::lock_guard<std::mutex> lock{mutex_};
        snapshot_ = std::move(replacement);
    }

private:
    mutable std::mutex mutex_;
    std::shared_ptr<const T> snapshot_;
};

} // namespace basalt::wire::config
