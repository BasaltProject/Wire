#include <basalt/wire/io/WorkerPool.hpp>

#include <doctest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <semaphore>
#include <stdexcept>
#include <utility>

using namespace basalt::wire::io;
using namespace std::chrono_literals;

TEST_CASE("worker admission is bounded and a canceled queued job never starts") {
    std::binary_semaphore entered{0}, canceled{0};
    std::mutex mutex;
    std::condition_variable_any gate;
    bool released = false;
    std::atomic<int> executions{0}, failures{0}, rejectedCallbacks{0};
    auto pool = WorkerPool::create(1, 1);
    REQUIRE(pool);
    const auto failed = [&](std::exception_ptr) { ++failures; };
    auto first = pool->submit(
        [&](std::stop_token stop) {
            ++executions;
            entered.release();
            std::unique_lock lock{mutex};
            gate.wait(lock, stop, [&] { return released; });
        },
        failed);
    REQUIRE(first);
    REQUIRE(entered.try_acquire_for(2s));
    auto second = pool->submit([&](std::stop_token) { ++executions; }, failed, [&] { canceled.release(); });
    REQUIRE(second);
    const auto rejected = pool->submit([&](std::stop_token) { ++executions; }, failed, [&] { ++rejectedCallbacks; });
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().kind == WorkerPoolError::Kind::Full);
    CHECK(pool->queued() == 1);
    second->request_stop();
    {
        std::lock_guard lock{mutex};
        released = true;
    }
    gate.notify_all();
    REQUIRE(canceled.try_acquire_for(2s));
    pool->join();
    CHECK(executions.load() == 1);
    CHECK(failures.load() == 0);
    CHECK(rejectedCallbacks.load() == 0);
}

TEST_CASE("a throwing task and failure callback do not kill a worker") {
    std::binary_semaphore survived{0};
    std::atomic<int> failures{0};
    auto pool = WorkerPool::create(1, 2);
    REQUIRE(pool);
    REQUIRE(pool->submit([](std::stop_token) { throw std::runtime_error{"synthetic failure"}; },
                         [&](std::exception_ptr error) {
                             CHECK(error != nullptr);
                             ++failures;
                             throw std::runtime_error{"failure callback threw"};
                         }));
    REQUIRE(pool->submit([&](std::stop_token) { survived.release(); }, [](std::exception_ptr) {}));
    REQUIRE(survived.try_acquire_for(2s));
    pool->join();
    CHECK(failures.load() == 1);
    CHECK(pool->failedTasks() == 2);
}
