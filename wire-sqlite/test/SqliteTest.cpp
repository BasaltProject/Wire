#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/sql/Migrations.hpp>

#include <doctest.h>

#include <array>

using namespace basalt::wire::sql;

TEST_CASE("migrations apply once, resume from the stored version and refuse a newer database") {
    auto database = Database::open(":memory:");
    REQUIRE(database);
    const std::array<Migration, 2> first = {
        {{1, "CREATE TABLE t(a INTEGER);"}, {2, "ALTER TABLE t ADD COLUMN b TEXT;"}}};
    REQUIRE(migrate(*database, first) == 2);
    REQUIRE(migrate(*database, first) == 2);
    CHECK(database->columns("t").size() == 2);

    const std::array<Migration, 1> older = {{{1, "CREATE TABLE t(a INTEGER);"}}};
    CHECK_FALSE(migrate(*database, older));

    const std::array<Migration, 2> broken = {{{3, "CREATE TABLE u(a INTEGER);"}, {4, "THIS IS NOT SQL;"}}};
    CHECK_FALSE(migrate(*database, broken));
    CHECK(database->tables().size() == 2);
    CHECK_FALSE(migrate(*database, std::array<Migration, 2>{{{2, "SELECT 1;"}, {2, "SELECT 1;"}}}));
}
