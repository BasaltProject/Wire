#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/permissions/PermissionChain.hpp>

#include <doctest.h>

using namespace basalt::wire::permissions;

TEST_CASE("the chain asks by priority, skips undefined answers and lets a provider build on the rest") {
    PermissionChain chain;
    const Subject steve = Subject::player("uuid", "Steve");
    auto groups = chain.provide({.id = "groups",
                                 .priority = kPriorityDefault,
                                 .check = [](const Query& query) { return tristateOf(query.node() == "a"); },
                                 .meta = {}});
    CHECK(chain.onlyDefault());
    auto plugin = chain.provide(
        {.id = "plugin",
         .priority = kPriorityPlugin,
         .check = [](const Query& query) { return query.node() == "b" ? Tristate::kTrue : Tristate::kUndefined; },
         .meta = [](const Subject&, std::string_view key,
                    const Context&) { return key == "prefix" ? std::optional<std::string>{"[vip]"} : std::nullopt; }});
    auto inverter = chain.provide({.id = "inverter",
                                   .priority = kPriorityAdapter,
                                   .check =
                                       [](const Query& query) {
                                           if (query.node() != "c")
                                               return Tristate::kUndefined;
                                           return query.rest() == Tristate::kTrue ? Tristate::kFalse : Tristate::kTrue;
                                       },
                                   .meta = {}});
    CHECK_FALSE(chain.onlyDefault());
    CHECK(chain.ids() == std::vector<std::string>{"inverter", "plugin", "groups"});
    CHECK(chain.has(steve, "a"));
    CHECK(chain.has(steve, "b"));
    CHECK(chain.has(steve, "c"));
    CHECK(chain.meta(steve, "prefix") == "[vip]");
    plugin.remove();
    CHECK_FALSE(chain.has(steve, "b"));
    CHECK_FALSE(chain.meta(steve, "prefix"));
    {
        [[maybe_unused]] auto gone = std::move(inverter);
    }
    CHECK(chain.ids() == std::vector<std::string>{"groups"});
    CHECK(chain.onlyDefault());

    auto loop =
        chain.provide({.id = "loop",
                       .priority = kPriorityPlugin,
                       .check = [&chain, steve](const Query& query) { return chain.check(steve, query.node()); },
                       .meta = {}});
    CHECK(chain.has(steve, "a"));
}
