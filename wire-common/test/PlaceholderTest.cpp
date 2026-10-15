#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/common/placeholder/PlaceholderCondition.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::wire::common::placeholder;

namespace {

struct Who {
    std::string name;
};

const PlaceholderSlot<Who> kWho{"who"};
const PlaceholderSlot<Who> kOther{"other"};

[[nodiscard]] PlaceholderRegistry serverRegistry() {
    PlaceholderRegistry registry;
    registry.registerProvider(
        "basalt_server",
        [](std::string_view params, const PlaceholderContext&) -> PlaceholderValue {
            if (params == "online") {
                return PlaceholderValue::of(std::int64_t{7});
            }
            if (params == "name") {
                return PlaceholderValue::of(std::string{"A Server"});
            }
            if (params == "whitelisted") {
                return PlaceholderValue::of(true);
            }
            if (params == "tps") {
                return PlaceholderValue::of(19.5);
            }
            if (params == "inventory") {
                return PlaceholderValue::unavailable("needs the inventory system");
            }
            if (params == "later") {
                return PlaceholderValue::unavailable();
            }
            return PlaceholderValue::unknown();
        },
        [] { return std::vector<std::string>{"online", "name", "whitelisted", "tps"}; });
    registry.registerProvider("basalt_player",
                              [](std::string_view params, const PlaceholderContext& context) -> PlaceholderValue {
                                  const Who* who = context.get(kWho);
                                  if (who == nullptr) {
                                      return PlaceholderValue::unavailable("there is no player here");
                                  }
                                  if (params == "name") {
                                      return PlaceholderValue::of(who->name);
                                  }
                                  return PlaceholderValue::unknown();
                              });
    return registry;
}

} // namespace

TEST_CASE("the longest registered identifier wins the name") {
    PlaceholderRegistry registry;
    registry.registerProvider("basalt", [](std::string_view params, const PlaceholderContext&) {
        return PlaceholderValue::of("short:" + std::string{params});
    });
    registry.registerProvider("basalt_server", [](std::string_view params, const PlaceholderContext&) {
        return PlaceholderValue::of("long:" + std::string{params});
    });

    CHECK(registry.resolve("basalt_server_online").text == "long:online");
    CHECK(registry.resolve("basalt_other").text == "short:other");
    CHECK(registry.resolve("basalt").text == "short:");
    CHECK(registry.resolve("basalt_server").text == "long:");
    CHECK(registry.resolve("basaltx_online").kind == PlaceholderKind::Unknown);
    CHECK(registry.resolve("").kind == PlaceholderKind::Unknown);

    CHECK(registry.resolve("BASALT_SERVER_Online").text == "long:online");
    registry.registerProvider("MiXeD", [](std::string_view, const PlaceholderContext&) {
        return PlaceholderValue::of(std::string{"mixed"});
    });
    CHECK(registry.resolve("mixed").text == "mixed");
    CHECK(registry.identifiers() == std::vector<std::string>{"basalt", "basalt_server", "mixed"});

    registry.registerProvider("basalt", [](std::string_view, const PlaceholderContext&) {
        return PlaceholderValue::of(std::string{"reloaded"});
    });
    CHECK(registry.resolve("basalt_other").text == "reloaded");
    CHECK(registry.identifiers().size() == 3);

    registry.unregisterProvider("BASALT");
    CHECK(registry.resolve("basalt_other").kind == PlaceholderKind::Unknown);
    CHECK(registry.resolve("basalt_server_online").text == "long:online");
}

TEST_CASE("what a per cent sign means in operator-written text") {
    const PlaceholderRegistry registry = serverRegistry();
    const Who steve{.name = "Steve"};
    const PlaceholderContext context = PlaceholderContext{}.with(kWho, &steve);

    CHECK(substitute("There are %basalt_server_online% players", registry, context) == "There are 7 players");
    CHECK(substitute("Hello %basalt_player_name%", registry, context) == "Hello Steve");
    CHECK(substitute("nothing here", registry, context) == "nothing here");

    CHECK(substitute("50% off, and 20% more", registry, context) == "50% off, and 20% more");
    CHECK(substitute("100%% sure", registry, context) == "100% sure");
    CHECK(substitute("%%", registry, context) == "%");
    CHECK(substitute("%unclosed", registry, context) == "%unclosed");
    CHECK(substitute("%%a%", registry, context) == "%a%");

    CHECK(substitute("%basalt_server_nothing%", registry, context) == "%basalt_server_nothing%");
    CHECK(substitute("%not_a_placeholder%", registry, context) == "%not_a_placeholder%");
    CHECK(substitute("[%not_a_placeholder%]", registry, context,
                     [](std::string_view, const PlaceholderValue&) { return std::optional<std::string>{""}; }) == "[]");
    CHECK(substitute("%basalt_server_online% of %basalt_server_nothing%", registry, context,
                     [](std::string_view name, const PlaceholderValue&) {
                         return std::optional<std::string>{"<" + std::string{name} + ">"};
                     }) == "7 of <basalt_server_nothing>");

    CHECK(substitute("%basalt_server_name%: %basalt_server_online%", registry, context) == "A Server: 7");
    CHECK(substitute("%basalt_server_online%%basalt_server_online%", registry, context) == "77");
}

TEST_CASE("numbers compare as numbers and everything else as text") {
    const PlaceholderRegistry registry = serverRegistry();
    const auto held = [&registry](const std::string& raw) { return PlaceholderCondition{raw}.evaluate(registry); };

    CHECK(held("%basalt_server_online% > 5").isTrue());
    CHECK(held("%basalt_server_online% >= 7").isTrue());
    CHECK(held("%basalt_server_online% < 5").outcome == ConditionOutcome::False);
    CHECK(held("%basalt_server_online% <= 7").isTrue());
    CHECK(held("%basalt_server_online% = 7").isTrue());
    CHECK(held("%basalt_server_online% == 7").isTrue());
    CHECK(held("%basalt_server_online% != 7").outcome == ConditionOutcome::False);
    CHECK(held("%basalt_server_online% = 7.0").isTrue());
    CHECK(held("07 = 7").isTrue());
    CHECK(held("%basalt_server_tps% > 19").isTrue());

    CHECK(held("%basalt_server_name% = a server").isTrue());
    CHECK(held("%basalt_server_name% != something else").isTrue());
    CHECK(held("%basalt_server_name% contains SERVER").isTrue());
    CHECK(held("%basalt_server_name% contains nothing").outcome == ConditionOutcome::False);
    CHECK(held("%basalt_server_name% matches A*r").isTrue());
    CHECK(held("%basalt_server_name% matches Z*").outcome == ConditionOutcome::False);

    const ConditionResult broken = held("%basalt_server_name% > 5");
    CHECK(broken.isUnresolvable());
    CHECK(broken.reason == "'A Server' is not a number, so '>' cannot compare it");
    CHECK(held("5 > %basalt_server_name%").reason == "'A Server' is not a number, so '>' cannot compare it");
    CHECK(held("12abc > 5").isUnresolvable());
    CHECK(held("inf > 5").isUnresolvable());
    CHECK(held("nan > 5").isUnresolvable());
    CHECK(held("infinity = infinity").isTrue());
    CHECK(held("+7 = 7").isTrue());
    CHECK(held("-7 < -6").isTrue());
    CHECK(held("7 = +7").isTrue());
}

TEST_CASE("an operator inside a placeholder is not an operator") {
    const PlaceholderRegistry registry = serverRegistry();
    const PlaceholderCondition dated{"%basalt_server_countdown_dd.MM.yyyy_01.01.2020% = x"};
    CHECK(dated.evaluate(registry).isUnresolvable());
    CHECK(dated.evaluate(registry).reason ==
          "%basalt_server_countdown_dd.MM.yyyy_01.01.2020% is not a placeholder this server knows");

    const ConditionResult inside = PlaceholderCondition{"%basalt_server_time_<h>% = 1"}.evaluate(registry);
    CHECK(inside.isUnresolvable());
    CHECK(inside.reason == "%basalt_server_time_<h>% is not a placeholder this server knows");

    PlaceholderRegistry registry2 = serverRegistry();
    registry2.registerProvider("rematches", [](std::string_view, const PlaceholderContext&) {
        return PlaceholderValue::of(std::string{"true"});
    });
    CHECK(PlaceholderCondition{"%rematches%"}.evaluate(registry2).isTrue());
    CHECK(PlaceholderCondition{"rematches"}.evaluate(registry2).outcome == ConditionOutcome::False);
    CHECK(PlaceholderCondition{"amatchesa"}.evaluate(registry2).outcome == ConditionOutcome::False);
    CHECK(PlaceholderCondition{"xcontains"}.evaluate(registry2).outcome == ConditionOutcome::False);
    CHECK(PlaceholderCondition{"ab containsb"}.evaluate(registry2).outcome == ConditionOutcome::False);
    CHECK(PlaceholderCondition{"ab contains b"}.evaluate(registry2).isTrue());
}
