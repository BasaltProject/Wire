#include <basalt/wire/io/TaskQueue.hpp>
#include <basalt/wire/io/WorkerPool.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace basalt::wire::io {
struct WorkerPool::Impl {
    struct Stats {
        std::atomic<std::size_t> failures{0};
    };

    struct Job {
        enum class State { Pending, Finished, Rejected };
        Work work;
        Failure failed;
        Canceled canceled;
        std::stop_source stop;
        std::shared_ptr<Stats> stats;
        std::atomic<State> state{State::Pending};

        void report(std::exception_ptr error) noexcept {
            ++stats->failures;
            try {
                failed(error);
            } catch (...) {
                ++stats->failures;
            }
        }

        void cancel() noexcept {
            state.store(State::Finished);
            if (canceled) {
                try {
                    canceled();
                } catch (...) {
                    report(std::current_exception());
                }
            }
        }

        ~Job() {
            if (state.load() == State::Pending) {
                cancel();
            }
        }
    };

    TaskQueue jobs;
    std::mutex mutex;
    std::condition_variable_any notification;
    std::stop_source stop;
    std::shared_ptr<Stats> stats = std::make_shared<Stats>();
    bool closed = false;
    std::vector<std::jthread> workers;

    Impl(std::size_t count, std::size_t capacity) : jobs(capacity) {
        workers.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            workers.emplace_back([this](std::stop_token threadStop) {
                std::stop_callback onThreadStop{threadStop, [this] { stop.request_stop(); }};
                while (true) {
                    std::unique_lock lock{mutex};
                    notification.wait(lock, stop.get_token(), [this] { return closed || jobs.size() != 0; });
                    if (closed || stop.stop_requested()) {
                        return;
                    }
                    auto task = jobs.pop();
                    lock.unlock();
                    if (task) {
                        (*task)();
                    }
                }
            });
        }
    }

    void requestStop() {
        {
            std::lock_guard lock{mutex};
            if (closed) {
                return;
            }
            closed = true;
        }
        stop.request_stop();
        jobs.close();
        notification.notify_all();
    }

    ~Impl() {
        requestStop();
        workers.clear();
    }
};

WorkerPool::WorkerPool(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

WorkerPool::WorkerPool(WorkerPool&& other) noexcept = default;
WorkerPool& WorkerPool::operator=(WorkerPool&& other) noexcept = default;
WorkerPool::~WorkerPool() = default;

std::expected<WorkerPool, WorkerPoolError> WorkerPool::create(std::size_t workers, std::size_t queueCapacity) {
    if (workers == 0 || queueCapacity == 0) {
        return std::unexpected{WorkerPoolError{WorkerPoolError::Kind::InvalidConfiguration}};
    }
    try {
        return WorkerPool{std::make_unique<Impl>(workers, queueCapacity)};
    } catch (const std::system_error& error) {
        return std::unexpected{WorkerPoolError{WorkerPoolError::Kind::StartFailed, error.code().value()}};
    }
}

std::expected<std::stop_source, WorkerPoolError> WorkerPool::submit(Work work, Failure failed, Canceled canceled) {
    if (!impl_) {
        return std::unexpected{WorkerPoolError{WorkerPoolError::Kind::Closed}};
    }
    if (!work || !failed) {
        return std::unexpected{WorkerPoolError{WorkerPoolError::Kind::InvalidTask}};
    }
    auto job = std::make_shared<Impl::Job>();
    job->work = std::move(work);
    job->failed = std::move(failed);
    job->canceled = std::move(canceled);
    job->stats = impl_->stats;
    const auto ticket = job->stop;
    {
        std::lock_guard lock{impl_->mutex};
        if (impl_->closed) {
            job->state = Impl::Job::State::Rejected;
            return std::unexpected{WorkerPoolError{WorkerPoolError::Kind::Closed}};
        }
        const auto posted = impl_->jobs.push([job, poolStop = impl_->stop.get_token()] {
            std::stop_callback onShutdown{poolStop, [&job] { job->stop.request_stop(); }};
            if (job->stop.stop_requested()) {
                job->cancel();
                return;
            }
            try {
                job->work(job->stop.get_token());
            } catch (...) {
                job->report(std::current_exception());
            }
            job->state = Impl::Job::State::Finished;
        });
        if (!posted) {
            job->state = Impl::Job::State::Rejected;
            return std::unexpected{WorkerPoolError{WorkerPoolError::Kind::Full}};
        }
    }
    impl_->notification.notify_one();
    return ticket;
}

void WorkerPool::requestStop() {
    if (impl_) {
        impl_->requestStop();
    }
}

void WorkerPool::join() {
    if (impl_) {
        impl_->requestStop();
        impl_->workers.clear();
    }
}

std::size_t WorkerPool::queued() const { return impl_ ? impl_->jobs.size() : 0; }

std::size_t WorkerPool::failedTasks() const noexcept { return impl_ ? impl_->stats->failures.load() : 0; }
} // namespace basalt::wire::io
