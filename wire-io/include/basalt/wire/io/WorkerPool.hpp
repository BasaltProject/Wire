#pragma once

#include <cstddef>
#include <exception>
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>

namespace basalt::wire::io {

struct WorkerPoolError {
    enum class Kind { InvalidConfiguration, InvalidTask, Full, Closed, StartFailed };
    Kind kind;
    int platformCode = 0;
};

/// Work must honor its stop token; callbacks run on workers and must post results back to their owner.
class WorkerPool {
public:
    using Work = std::function<void(std::stop_token)>;
    using Failure = std::function<void(std::exception_ptr)>;
    using Canceled = std::function<void()>;

    [[nodiscard]] static std::expected<WorkerPool, WorkerPoolError> create(std::size_t workers,
                                                                           std::size_t queueCapacity);
    WorkerPool(WorkerPool&& other) noexcept;
    WorkerPool& operator=(WorkerPool&& other) noexcept;
    ~WorkerPool();
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    [[nodiscard]] std::expected<std::stop_source, WorkerPoolError> submit(Work work, Failure failed,
                                                                          Canceled canceled = {});
    /// Safe from a worker or callback; does not join threads.
    void requestStop();
    /// Coordinator only. Joins running work; callbacks must not destroy/join their own pool.
    void join();
    [[nodiscard]] std::size_t queued() const;
    [[nodiscard]] std::size_t failedTasks() const noexcept;

private:
    struct Impl;
    explicit WorkerPool(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace basalt::wire::io
