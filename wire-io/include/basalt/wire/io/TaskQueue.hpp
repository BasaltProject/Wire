#pragma once

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>

namespace basalt::wire::io {

enum class TaskQueueError { Full, Closed, InvalidTask };

/// Bounded multi-producer mailbox. Tasks are popped, then executed outside its lock by their owner.
/// wakeup/worker notification belongs to the consumer; this class does not execute user callbacks.
class TaskQueue {
public:
    using Task = std::function<void()>;
    explicit TaskQueue(std::size_t capacity);
    ~TaskQueue();
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    [[nodiscard]] std::expected<void, TaskQueueError> push(Task task);
    [[nodiscard]] std::optional<Task> pop();
    [[nodiscard]] std::size_t size() const;
    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace basalt::wire::io
