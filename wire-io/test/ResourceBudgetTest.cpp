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
