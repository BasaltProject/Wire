#include <basalt/wire/io/BudgetMemoryResource.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <thread>
#include <vector>

using namespace basalt::wire::io;

TEST_CASE("buffer allocation checks global and owner quotas before growth") {
    ResourceBudget global{4096}, local{2500};
    BudgetMemoryResource resource{global, local};
    {
        std::pmr::vector<std::uint8_t> bytes{&resource};
        bytes.reserve(1024);
        bytes.push_back(42);
        CHECK(global.used() == bytes.capacity());
        CHECK(local.used() == bytes.capacity());
        CHECK_THROWS_AS(bytes.reserve(2048), std::bad_alloc);
        CHECK(bytes.size() == 1);
        CHECK(bytes[0] == 42);
        CHECK(global.used() == bytes.capacity());
        CHECK(local.used() == bytes.capacity());
    }
    CHECK(global.used() == 0);
    CHECK(local.used() == 0);
    ResourceBudget tinyGlobal{128}, ampleLocal{4096};
    BudgetMemoryResource limited{tinyGlobal, ampleLocal};
    CHECK_THROWS_AS((void)limited.allocate(256), std::bad_alloc);
    CHECK(tinyGlobal.used() == 0);
    CHECK(ampleLocal.used() == 0);
}

TEST_CASE("managed packet buffers preserve quota on copies and fail reservation without losing bytes") {
    ResourceBudget global{4096}, local{2500};
    BudgetMemoryResource resource{global, local};
    {
        ByteBuffer buffer{1024, resource};
        buffer.writeInt(42);
        CHECK_FALSE(buffer.tryReserve(2048));
        CHECK(buffer.readInt() == 42);
        buffer.discardReadBytes();
        buffer.writeInt(99);
        auto copy = buffer;
        CHECK(global.used() == buffer.capacity() + copy.capacity());
        CHECK(local.used() == global.used());
        CHECK(copy.readInt() == 99);
        ByteBuffer assigned{0};
        assigned = buffer;
        CHECK(global.used() == buffer.capacity() + copy.capacity() + assigned.capacity());
        ByteBuffer managed{64, resource};
        managed = ByteBuffer::wrap({1, 2, 3});
        CHECK(global.used() == buffer.capacity() + copy.capacity() + assigned.capacity() + managed.capacity());
        CHECK_FALSE(buffer.tryReserve(std::numeric_limits<std::size_t>::max()));
    }
    CHECK(global.used() == 0);
    CHECK(local.used() == 0);
    std::vector<std::uint8_t> bytes{1, 2, 3};
    const auto* original = bytes.data();
    auto adopted = ByteBuffer::wrap(std::move(bytes));
    CHECK(adopted.readable().data() == original);
}
