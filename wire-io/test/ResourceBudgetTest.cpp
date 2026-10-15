#include <basalt/wire/io/ResourceBudget.hpp>

#include <doctest.h>

#include <atomic>
#include <barrier>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

using basalt::wire::io::ResourceBudget;

TEST_CASE("resource reservations release capacity on resize, move and scope exit") {
    ResourceBudget budget{10};
    auto first = budget.acquire(6);
    auto second = budget.acquire(4);
    REQUIRE(first);
    REQUIRE(second);
    CHECK_FALSE(budget.acquire(1));
    CHECK_FALSE(first->resize(7));
    CHECK(first->size() == 6);
    CHECK(budget.used() == 10);
    REQUIRE(second->resize(2));
    REQUIRE(first->resize(8));
    *second = std::move(*first);
    CHECK(budget.used() == 8);
    CHECK_FALSE(first->resize(1));
    second->reset();
    second->reset();
    CHECK(budget.used() == 0);
    {
        auto temporary = budget.acquire(10);
        REQUIRE(temporary);
    }
    CHECK(budget.used() == 0);
}

TEST_CASE("resource accounting cannot overflow and reservations retain budget lifetime") {
    ResourceBudget budget{std::numeric_limits<std::size_t>::max()};
    auto full = budget.acquire(budget.limit());
    REQUIRE(full);
    CHECK_FALSE(budget.acquire(1));
    full->reset();
    auto survivor = [] {
        ResourceBudget local{3};
        auto copy = local;
        auto held = copy.acquire(2);
        CHECK(local.used() == 2);
        return held;
    }();
    REQUIRE(survivor);
    CHECK(survivor->resize(3));
    CHECK_FALSE(survivor->resize(4));
    ResourceBudget empty{0};
    CHECK(empty.acquire(0).has_value());
    CHECK_FALSE(empty.acquire(1));
}

TEST_CASE("owners compete for a single global resource limit") {
    constexpr std::size_t capacity = 4;
    ResourceBudget budget{capacity};
    std::barrier phase{8};
    std::atomic<std::size_t> successes{0};
    std::atomic<bool> exceeded{false};
    std::vector<std::jthread> owners;
    for (int i = 0; i < 8; ++i) {
        owners.emplace_back([&, shared = budget] {
            phase.arrive_and_wait();
            auto reservation = shared.acquire(1);
            if (reservation)
                successes.fetch_add(1);
            if (shared.used() > capacity)
                exceeded.store(true);
            phase.arrive_and_wait();
        });
    }
    owners.clear();
    CHECK(successes.load() == capacity);
    CHECK_FALSE(exceeded.load());
    CHECK(budget.used() == 0);
}
