#include <basalt/wire/io/EventLoop.hpp>
#include <basalt/wire/io/TaskQueue.hpp>
#include <basalt/wire/io/TimerQueue.hpp>

#include <doctest.h>

#include <atomic>
#include <chrono>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

using namespace basalt::wire::io;
using namespace std::chrono_literals;

TEST_CASE("timer deadlines are ordered, cancelable and rounded up for readiness waits") {
    TimerQueue timers{3};
    const TimerQueue::TimePoint start{};
    const auto late = timers.schedule(start + 10ms, 11);
    const auto early = timers.schedule(start + 1ms, 22);
    const auto canceled = timers.schedule(start + 5ms, 33);
    REQUIRE(late);
    REQUIRE(early);
    REQUIRE(canceled);
    CHECK_FALSE(timers.schedule(start, 44));
    CHECK(timers.nextTimeout(start + 500us) == 1);
    REQUIRE(timers.cancel(*canceled));
    CHECK_FALSE(timers.cancel(*canceled));
    REQUIRE(timers.reschedule(*late, start + 750us));
    CHECK(timers.nextTimeout(start + 500us) == 1);
    auto ready = timers.expire(start + 750us);
    REQUIRE(ready.size() == 1);
    CHECK(ready.front().id == *late);
    CHECK(ready.front().token == 11);
    CHECK_FALSE(timers.reschedule(*late, start));
    ready = timers.expire(start + 10ms);
    REQUIRE(ready.size() == 1);
    CHECK(ready.front().id == *early);
    CHECK(ready.front().token == 22);
    CHECK(timers.size() == 0);
    CHECK_FALSE(timers.nextTimeout(start));
}

TEST_CASE("canceling long timers immediately frees bounded capacity") {
    TimerQueue timers{1};
    const TimerQueue::TimePoint start{};
    for (std::uint64_t i = 0; i < 1000; ++i) {
        const auto id = timers.schedule(start + 24h, i);
        REQUIRE(id);
        REQUIRE(timers.cancel(*id));
        CHECK(timers.size() == 0);
    }
    CHECK(timers.expire(start + 48h).empty());
    const auto future = timers.schedule(start + 24h * 100, 1);
    REQUIRE(future);
    CHECK(timers.nextTimeout(start) == std::numeric_limits<int>::max());
}

TEST_CASE("rescheduling both directions and deleting interior timers preserves deadline order") {
    TimerQueue timers{64};
    const TimerQueue::TimePoint start{};
    std::vector<std::uint64_t> ids;
    for (std::uint64_t i = 0; i < 64; ++i) {
        const auto id = timers.schedule(start + std::chrono::milliseconds{static_cast<int>(63 - i)}, i);
        REQUIRE(id);
        ids.push_back(*id);
    }
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i % 3 == 0) {
            REQUIRE(timers.cancel(ids[i]));
        } else {
            REQUIRE(timers.reschedule(ids[i], start + std::chrono::milliseconds{static_cast<int>(i)}));
        }
    }
    const auto ready = timers.expire(start + 100ms);
    REQUIRE(ready.size() == 42);
    std::size_t index = 0;
    for (std::uint64_t i = 0; i < 64; ++i) {
        if (i % 3 != 0) {
            CHECK(ready[index++].token == i);
        }
    }
    CHECK(timers.size() == 0);
}

TEST_CASE("the owner mailbox applies bounded admission and FIFO execution") {
    TaskQueue queue{2};
    std::vector<int> calls;
    REQUIRE(queue.push([&] { calls.push_back(1); }));
    REQUIRE(queue.push([&] { calls.push_back(2); }));
    const auto full = queue.push([] {});
    REQUIRE_FALSE(full);
    CHECK(full.error() == TaskQueueError::Full);
    auto task = queue.pop();
    REQUIRE(task);
    (*task)();
    REQUIRE(queue.push([&] { calls.push_back(3); }));
    while ((task = queue.pop())) {
        (*task)();
    }
    CHECK(calls == std::vector<int>{1, 2, 3});
    CHECK(queue.size() == 0);
    queue.close();
    const auto closed = queue.push([] {});
    REQUIRE_FALSE(closed);
    CHECK(closed.error() == TaskQueueError::Closed);
    CHECK_FALSE(queue.pop());
}

TEST_CASE("multiple workers publish results exactly once to an owner mailbox") {
    TaskQueue queue{128};
    std::atomic<int> completed{0};
    std::atomic<int> rejected{0};
    std::vector<std::jthread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&] {
            for (int j = 0; j < 16; ++j) {
                if (!queue.push([&] { ++completed; })) {
                    ++rejected;
                }
            }
        });
    }
    workers.clear();
    while (const auto task = queue.pop()) {
        (*task)();
    }
    CHECK(completed.load() == 64);
    CHECK(rejected.load() == 0);
}

TEST_CASE("closing a mailbox destroys captures outside its lock") {
    TaskQueue queue{1};
    std::atomic<bool> observedClosed{false};

    struct Cleanup {
        TaskQueue* queue;
        std::atomic<bool>* closed;

        ~Cleanup() {
            const auto result = queue->push([] {});
            closed->store(!result && result.error() == TaskQueueError::Closed);
        }
    };

    auto cleanup = std::make_shared<Cleanup>(&queue, &observedClosed);
    REQUIRE(queue.push([capture = cleanup] { static_cast<void>(capture); }));
    // Drop the external reference so close owns the last capture.
    cleanup.reset();
    queue.close();
    CHECK(observedClosed.load());
}

TEST_CASE("worker results wake the owner and execute outside the mailbox") {
    EventLoop loop;
    TaskQueue mailbox{1};
    int calls = 0;
    std::atomic<bool> posted{false};
    std::jthread worker([&] {
        posted.store(mailbox.push([&] { ++calls; }).has_value());
        static_cast<void>(loop.wake());
    });
    // A missing wakeup is caught by the CTest timeout instead of passing after a timed wait.
    const auto ready = loop.wait();
    REQUIRE(ready);
    worker.join();
    REQUIRE(posted.load());
    CHECK(calls == 0);
    const auto task = mailbox.pop();
    REQUIRE(task);
    (*task)();
    CHECK(calls == 1);
}

TEST_CASE("zero-capacity mailboxes reject work and empty callbacks are invalid") {
    TaskQueue queue{0};
    const auto full = queue.push([] {});
    REQUIRE_FALSE(full);
    CHECK(full.error() == TaskQueueError::Full);
    const auto invalid = queue.push({});
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error() == TaskQueueError::InvalidTask);
    CHECK_FALSE(queue.pop());
}
