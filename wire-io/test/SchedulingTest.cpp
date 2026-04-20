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
