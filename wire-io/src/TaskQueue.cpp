#include <basalt/wire/io/TaskQueue.hpp>

#include <mutex>
#include <utility>
#include <vector>

namespace basalt::wire::io {
struct TaskQueue::Impl {
    explicit Impl(std::size_t capacity) : tasks(capacity) {}

    mutable std::mutex mutex;
    std::vector<Task> tasks;
    std::size_t head = 0;
    std::size_t used = 0;
    bool closed = false;
};

TaskQueue::TaskQueue(std::size_t capacity) : impl_(std::make_unique<Impl>(capacity)) {}

TaskQueue::~TaskQueue() { close(); }

std::expected<void, TaskQueueError> TaskQueue::push(Task task) {
    if (!task) {
        return std::unexpected{TaskQueueError::InvalidTask};
    }
    std::lock_guard lock{impl_->mutex};
    if (impl_->closed) {
        return std::unexpected{TaskQueueError::Closed};
    }
    if (impl_->used == impl_->tasks.size()) {
        return std::unexpected{TaskQueueError::Full};
    }
    impl_->tasks[(impl_->head + impl_->used) % impl_->tasks.size()] = std::move(task);
    ++impl_->used;
    return {};
}

std::optional<TaskQueue::Task> TaskQueue::pop() {
    std::lock_guard lock{impl_->mutex};
    if (impl_->used == 0) {
        return std::nullopt;
    }
    Task task = std::move(impl_->tasks[impl_->head]);
    impl_->head = (impl_->head + 1) % impl_->tasks.size();
    --impl_->used;
    return task;
}

std::size_t TaskQueue::size() const {
    std::lock_guard lock{impl_->mutex};
    return impl_->used;
}

void TaskQueue::close() {
    std::vector<Task> abandoned;
    {
        std::lock_guard lock{impl_->mutex};
        impl_->closed = true;
        impl_->used = 0;
        abandoned.swap(impl_->tasks);
    }
    // Captures may release resources or try to enqueue cleanup. Never destroy them under the lock.
}
} // namespace basalt::wire::io
