#include <basalt/wire/io/TimerQueue.hpp>

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace basalt::wire::io {
struct TimerQueue::Impl {
    struct Timer {
        TimePoint deadline;
        std::uint64_t id;
        std::uint64_t token;
    };

    explicit Impl(std::size_t limit) : capacity(limit) {
        timers.reserve(limit);
        ready.reserve(limit);
        indices.reserve(limit);
    }

    std::size_t capacity;
    std::uint64_t nextId = 1;
    std::vector<Timer> timers;
    std::unordered_map<std::uint64_t, std::size_t> indices;
    std::vector<TimerReady> ready;

    bool earlier(std::size_t first, std::size_t second) const {
        const Timer& a = timers[first];
        const Timer& b = timers[second];
        return a.deadline < b.deadline || (a.deadline == b.deadline && a.id < b.id);
    }

    void swap(std::size_t first, std::size_t second) {
        std::swap(timers[first], timers[second]);
        indices.at(timers[first].id) = first;
        indices.at(timers[second].id) = second;
    }

    void up(std::size_t index) {
        while (index > 0) {
            const std::size_t parent = (index - 1) / 2;
            if (!earlier(index, parent)) {
                break;
            }
            swap(index, parent);
            index = parent;
        }
    }

    void down(std::size_t index) {
        while (index * 2 + 1 < timers.size()) {
            std::size_t child = index * 2 + 1;
            if (child + 1 < timers.size() && earlier(child + 1, child)) {
                ++child;
            }
            if (!earlier(child, index)) {
                break;
            }
            swap(child, index);
            index = child;
        }
    }

    void fix(std::size_t index) {
        if (index > 0 && earlier(index, (index - 1) / 2)) {
            up(index);
        } else {
            down(index);
        }
    }

    void remove(std::size_t index) {
        const std::uint64_t id = timers[index].id;
        if (index + 1 != timers.size()) {
            swap(index, timers.size() - 1);
        }
        timers.pop_back();
        indices.erase(id);
        if (index < timers.size()) {
            fix(index);
        }
    }
};

TimerQueue::TimerQueue(std::size_t capacity) : impl_(std::make_unique<Impl>(capacity)) {}

TimerQueue::~TimerQueue() = default;

std::size_t TimerQueue::size() const noexcept { return impl_->timers.size(); }

std::optional<std::uint64_t> TimerQueue::schedule(TimePoint deadline, std::uint64_t token) {
    if (impl_->timers.size() == impl_->capacity || impl_->nextId == 0) {
        return std::nullopt;
    }
    const std::uint64_t id = impl_->nextId++;
    impl_->timers.push_back({deadline, id, token});
    impl_->indices.emplace(id, impl_->timers.size() - 1);
    impl_->up(impl_->timers.size() - 1);
    return id;
}

bool TimerQueue::reschedule(std::uint64_t id, TimePoint deadline) {
    const auto found = impl_->indices.find(id);
    if (found == impl_->indices.end()) {
        return false;
    }
    const std::size_t index = found->second;
    impl_->timers[index].deadline = deadline;
    impl_->fix(index);
    return true;
}

bool TimerQueue::cancel(std::uint64_t id) {
    const auto found = impl_->indices.find(id);
    if (found == impl_->indices.end()) {
        return false;
    }
    impl_->remove(found->second);
    return true;
}

std::optional<int> TimerQueue::nextTimeout(TimePoint now) const {
    if (impl_->timers.empty()) {
        return std::nullopt;
    }
    const auto duration = std::chrono::ceil<std::chrono::milliseconds>(impl_->timers.front().deadline - now);
    return static_cast<int>(std::clamp<std::int64_t>(duration.count(), 0, std::numeric_limits<int>::max()));
}

std::span<const TimerReady> TimerQueue::expire(TimePoint now) {
    impl_->ready.clear();
    while (!impl_->timers.empty() && impl_->timers.front().deadline <= now) {
        const Impl::Timer first = impl_->timers.front();
        impl_->ready.push_back({first.id, first.token});
        impl_->remove(0);
    }
    return impl_->ready;
}
} // namespace basalt::wire::io
