#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/plugins/CommandSpec.hpp>

#include <doctest.h>

using namespace basalt::wire::plugins;

TEST_CASE("a command spec splits into literals and typed arguments and rejects misplaced words") {
    const auto spec = CommandSpec::parse("coins top [limit: int 1..25]", "example.top");
    REQUIRE(spec);
    CHECK(spec->literals == std::vector<std::string>{"coins", "top"});
    REQUIRE(spec->arguments.size() == 1);
    CHECK(spec->arguments[0].type == CommandArgumentType::Int);
    CHECK(spec->arguments[0].optional);
    CHECK(spec->arguments[0].min == 1);
    CHECK(spec->arguments[0].max == 25);
    CHECK(spec->usage() == "coins top [limit: int 1..25]");
    CHECK_FALSE(CommandSpec::parse("coins [who: word] top"));
    CHECK_FALSE(CommandSpec::parse("<who: word>"));
}
