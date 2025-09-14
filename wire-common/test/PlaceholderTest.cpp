#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/common/placeholder/PlaceholderCondition.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <doctest.h>

#include <string>
#include <vector>

using namespace basalt::wire::common::placeholder;

namespace {

/// Something for a context to carry, standing in for the player or the world a real one holds.
struct Who {
    std::string name;
};

const PlaceholderSlot<Who> kWho{"who"};
const PlaceholderSlot<Who> kOther{"other"};

/// A registry answering a fixed family, which is what every provider looks like from here.
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

TEST_CASE("a value says which of the two failures it is") {
    CHECK(PlaceholderValue::of(std::string{"x"}).isText());
    CHECK(PlaceholderValue::of(std::int64_t{12}).text == "12");
    CHECK(PlaceholderValue::of(true).text == "true");
    CHECK(PlaceholderValue::of(false).text == "false");
    // Written as the frozen Kotlin wrote a Double, decimal point and all.
    CHECK(PlaceholderValue::of(19.5).text == "19.5");
    CHECK(PlaceholderValue::of(20.0).text == "20.0");

    const PlaceholderValue unknown = PlaceholderValue::unknown();
    CHECK_FALSE(unknown.isText());
    CHECK(unknown.textOrNull() == nullptr);
    CHECK(unknown.kind == PlaceholderKind::Unknown);

    // Nobody registered it against a name that exists but cannot answer here: different mistakes,
    // different messages.
    const PlaceholderValue unavailable = PlaceholderValue::unavailable("needs a player");
    CHECK(unavailable.kind == PlaceholderKind::Unavailable);
    REQUIRE(unavailable.reason.has_value());
    CHECK(*unavailable.reason == "needs a player");
    CHECK_FALSE(PlaceholderValue::unavailable().reason.has_value());
}

TEST_CASE("a context carries what a slot names and nothing else") {
    const Who steve{.name = "Steve"};
    const Who alex{.name = "Alex"};

    const PlaceholderContext empty;
    CHECK(empty.get(kWho) == nullptr);
    CHECK(empty.size() == 0);

    const PlaceholderContext one = empty.with(kWho, &steve);
    REQUIRE(one.get(kWho) != nullptr);
    CHECK(one.get(kWho)->name == "Steve");
    CHECK(one.get(kOther) == nullptr);
    // Adding does not change what it was added to: a context is built once and passed down.
    CHECK(empty.get(kWho) == nullptr);

    const PlaceholderContext two = one.with(kOther, &alex);
    CHECK(two.get(kWho)->name == "Steve");
    CHECK(two.get(kOther)->name == "Alex");
    CHECK(two.size() == 2);

    // Nothing to put in is not an entry, so a caller need not check first.
    CHECK(one.with(kOther, static_cast<const Who*>(nullptr)).size() == 1);
    // And the same slot again replaces rather than shadowing.
    CHECK(one.with(kWho, &alex).get(kWho)->name == "Alex");
    CHECK(one.with(kWho, &alex).size() == 1);
}

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
    // An identifier is matched at an underscore, not in the middle of a word: `basaltx` is not
    // `basalt`'s.
    CHECK(registry.resolve("basaltx_online").kind == PlaceholderKind::Unknown);
    CHECK(registry.resolve("").kind == PlaceholderKind::Unknown);

    // The case an operator wrote it in does not matter.
    CHECK(registry.resolve("BASALT_SERVER_Online").text == "long:online");
    registry.registerProvider("MiXeD", [](std::string_view, const PlaceholderContext&) {
        return PlaceholderValue::of(std::string{"mixed"});
    });
    CHECK(registry.resolve("mixed").text == "mixed");
    CHECK(registry.identifiers() == std::vector<std::string>{"basalt", "basalt_server", "mixed"});

    // Registering twice replaces, which is how a plugin reloads its own.
    registry.registerProvider("basalt", [](std::string_view, const PlaceholderContext&) {
        return PlaceholderValue::of(std::string{"reloaded"});
    });
    CHECK(registry.resolve("basalt_other").text == "reloaded");
    CHECK(registry.identifiers().size() == 3);

    registry.unregisterProvider("BASALT");
    CHECK(registry.resolve("basalt_other").kind == PlaceholderKind::Unknown);
    CHECK(registry.resolve("basalt_server_online").text == "long:online");
}

TEST_CASE("a provider that lists itself, and one that does not") {
    const PlaceholderRegistry registry = serverRegistry();
    CHECK(registry.namesOf("basalt_server") == std::vector<std::string>{"online", "name", "whitelisted", "tps"});
    CHECK(registry.namesOf("basalt_player").empty());
    CHECK(registry.namesOf("nothing").empty());

    REQUIRE(registry.identifierFor("basalt_server_online").has_value());
    CHECK(*registry.identifierFor("basalt_server_online") == "basalt_server");
    CHECK_FALSE(registry.identifierFor("nothing_at_all").has_value());
}

TEST_CASE("what a per cent sign means in operator-written text") {
    const PlaceholderRegistry registry = serverRegistry();
    const Who steve{.name = "Steve"};
    const PlaceholderContext context = PlaceholderContext{}.with(kWho, &steve);

    CHECK(substitute("There are %basalt_server_online% players", registry, context) == "There are 7 players");
    CHECK(substitute("Hello %basalt_player_name%", registry, context) == "Hello Steve");
    CHECK(substitute("nothing here", registry, context) == "nothing here");

    // Prose is safe: a name may not contain whitespace, so a sale is not a placeholder.
    CHECK(substitute("50% off, and 20% more", registry, context) == "50% off, and 20% more");
    // And `%%` is a literal per cent sign.
    CHECK(substitute("100%% sure", registry, context) == "100% sure");
    CHECK(substitute("%%", registry, context) == "%");
    // An unclosed one is text.
    CHECK(substitute("%unclosed", registry, context) == "%unclosed");
    CHECK(substitute("%%a%", registry, context) == "%a%");

    // An unanswerable name is left as written, which is what tells the operator where to look.
    CHECK(substitute("%basalt_server_nothing%", registry, context) == "%basalt_server_nothing%");
    CHECK(substitute("%not_a_placeholder%", registry, context) == "%not_a_placeholder%");
    // Unless the caller says otherwise, which is right for a line strangers read.
    CHECK(substitute("[%not_a_placeholder%]", registry, context,
                     [](std::string_view, const PlaceholderValue&) { return std::optional<std::string>{""}; }) == "[]");
    CHECK(substitute("%basalt_server_online% of %basalt_server_nothing%", registry, context,
                     [](std::string_view name, const PlaceholderValue&) {
                         return std::optional<std::string>{"<" + std::string{name} + ">"};
                     }) == "7 of <basalt_server_nothing>");

    // Several in one line, back to back.
    CHECK(substitute("%basalt_server_name%: %basalt_server_online%", registry, context) == "A Server: 7");
    CHECK(substitute("%basalt_server_online%%basalt_server_online%", registry, context) == "77");
}

TEST_CASE("the names a piece of text will need, worked out up front") {
    CHECK(namesIn("nothing here").empty());
    CHECK(namesIn("%a% and %b%") == std::vector<std::string>{"a", "b"});
    CHECK(namesIn("100%% sure").empty());
    CHECK(namesIn("50% off, 20% more").empty());
    CHECK(namesIn("%unclosed").empty());
    CHECK(namesIn("%%%a%") == std::vector<std::string>{"a"});
    // The literal per cent sign is consumed whole, so what follows it starts a name rather than
    // finishing the one the second sign would otherwise have opened.
    CHECK(namesIn("%%a%").empty());
    CHECK(namesIn("%%a%b%") == std::vector<std::string>{"b"});
}

TEST_CASE("what a star stands for") {
    CHECK(globMatches("play.example.com", "play.example.com"));
    CHECK(globMatches("PLAY.example.com", "play.EXAMPLE.com"));
    CHECK_FALSE(globMatches("play.example.com", "play.example.org"));

    CHECK(globMatches("*", "anything at all"));
    CHECK(globMatches("*", ""));
    CHECK(globMatches("*.example.com", "play.example.com"));
    CHECK(globMatches("play.*", "play.example.com"));
    CHECK(globMatches("play.*.com", "play.example.com"));
    CHECK_FALSE(globMatches("play.*.com", "play.example.org"));
    CHECK(globMatches("a*b*c", "axxbyyc"));
    CHECK_FALSE(globMatches("a*b*c", "acb"));

    // The trailing literal has to fit after everything already matched, or the same characters
    // would count as both: the star may stand for nothing, but not for what is already spoken for.
    CHECK(globMatches("a*bc", "abc"));
    CHECK_FALSE(globMatches("ab*bc", "abc"));
    CHECK_FALSE(globMatches("abc*abc", "abc"));
    // The same two characters cannot be the middle and the end at once.
    CHECK_FALSE(globMatches("*ab*ab", "ab"));
    CHECK(globMatches("*ab*ab", "abab"));
    // Two stars in a row stand for the same thing one does.
    CHECK(globMatches("a**c", "abc"));
    CHECK(globMatches("a**c", "ac"));
}

TEST_CASE("a condition with no operator is true when it says true") {
    const PlaceholderRegistry registry = serverRegistry();
    CHECK(PlaceholderCondition{"%basalt_server_whitelisted%"}.evaluate(registry).isTrue());
    CHECK(PlaceholderCondition{"true"}.evaluate(registry).isTrue());
    CHECK(PlaceholderCondition{"TRUE"}.evaluate(registry).isTrue());
    CHECK(PlaceholderCondition{"  true  "}.evaluate(registry).isTrue());
    CHECK_FALSE(PlaceholderCondition{"false"}.evaluate(registry).isTrue());
    CHECK_FALSE(PlaceholderCondition{"anything"}.evaluate(registry).isTrue());
    CHECK(PlaceholderCondition{"false"}.evaluate(registry).outcome == ConditionOutcome::False);
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
    // Numeric, so the spelling of the number does not matter.
    CHECK(held("%basalt_server_online% = 7.0").isTrue());
    CHECK(held("07 = 7").isTrue());
    CHECK(held("%basalt_server_tps% > 19").isTrue());

    // Text otherwise, and case-insensitively.
    CHECK(held("%basalt_server_name% = a server").isTrue());
    CHECK(held("%basalt_server_name% != something else").isTrue());
    CHECK(held("%basalt_server_name% contains SERVER").isTrue());
    CHECK(held("%basalt_server_name% contains nothing").outcome == ConditionOutcome::False);
    CHECK(held("%basalt_server_name% matches A*r").isTrue());
    CHECK(held("%basalt_server_name% matches Z*").outcome == ConditionOutcome::False);

    // An ordering operator on text is a mistake, and it says which side it choked on.
    const ConditionResult broken = held("%basalt_server_name% > 5");
    CHECK(broken.isUnresolvable());
    CHECK(broken.reason == "'A Server' is not a number, so '>' cannot compare it");
    CHECK(held("5 > %basalt_server_name%").reason == "'A Server' is not a number, so '>' cannot compare it");
    // Half a number is not a number.
    CHECK(held("12abc > 5").isUnresolvable());
    // Nor is a word the platform happens to read as one: an infinity or a NaN compares false
    // against everything, which is not what an operator writing the file would read it as.
    CHECK(held("inf > 5").isUnresolvable());
    CHECK(held("nan > 5").isUnresolvable());
    CHECK(held("infinity = infinity").isTrue());
    // A signed number is a number, on either side.
    CHECK(held("+7 = 7").isTrue());
    CHECK(held("-7 < -6").isTrue());
    CHECK(held("7 = +7").isTrue());
}

TEST_CASE("an operator inside a placeholder is not an operator") {
    const PlaceholderRegistry registry = serverRegistry();
    // A placeholder's argument is free text: the dots and dashes of a date are not comparisons.
    const PlaceholderCondition dated{"%basalt_server_countdown_dd.MM.yyyy_01.01.2020% = x"};
    CHECK(dated.evaluate(registry).isUnresolvable());
    CHECK(dated.evaluate(registry).reason ==
          "%basalt_server_countdown_dd.MM.yyyy_01.01.2020% is not a placeholder this server knows");

    // An operator character inside a placeholder is part of its argument, so what surrounds it is
    // not a comparison: the line is one placeholder against one word, and the name it reports is
    // the whole placeholder.
    const ConditionResult inside = PlaceholderCondition{"%basalt_server_time_<h>% = 1"}.evaluate(registry);
    CHECK(inside.isUnresolvable());
    CHECK(inside.reason == "%basalt_server_time_<h>% is not a placeholder this server knows");

    // A word operator has to be a word: one inside another word is part of that word.
    PlaceholderRegistry registry2 = serverRegistry();
    registry2.registerProvider("rematches", [](std::string_view, const PlaceholderContext&) {
        return PlaceholderValue::of(std::string{"true"});
    });
    CHECK(PlaceholderCondition{"%rematches%"}.evaluate(registry2).isTrue());
    CHECK(PlaceholderCondition{"rematches"}.evaluate(registry2).outcome == ConditionOutcome::False);
    // Neither of these is a comparison, and each would be a true one if it were read as one.
    CHECK(PlaceholderCondition{"amatchesa"}.evaluate(registry2).outcome == ConditionOutcome::False);
    CHECK(PlaceholderCondition{"xcontains"}.evaluate(registry2).outcome == ConditionOutcome::False);
    CHECK(PlaceholderCondition{"ab containsb"}.evaluate(registry2).outcome == ConditionOutcome::False);
    // With the word standing alone it is the operator it looks like.
    CHECK(PlaceholderCondition{"ab contains b"}.evaluate(registry2).isTrue());
}

TEST_CASE("an unresolvable condition says which mistake it was") {
    const PlaceholderRegistry registry = serverRegistry();
    const auto reasonOf = [&registry](const std::string& raw) {
        return PlaceholderCondition{raw}.evaluate(registry).reason;
    };

    // Nobody registered it.
    CHECK(reasonOf("%nothing% = 1") == "%nothing% is not a placeholder this server knows");
    // Registered, but this situation cannot answer it, and it says what is missing.
    CHECK(reasonOf("%basalt_server_inventory% = 1") ==
          "%basalt_server_inventory% cannot be used here: needs the inventory system");
    // Unavailable without a reason still says it is not the same as unknown.
    CHECK(reasonOf("%basalt_server_later% = 1") == "%basalt_server_later% cannot be answered here");
    // The first failure is the one reported, left side first.
    CHECK(reasonOf("%nothing% = %basalt_server_inventory%") == "%nothing% is not a placeholder this server knows");
    CHECK(reasonOf("1 = %nothing%") == "%nothing% is not a placeholder this server knows");

    // A player placeholder outside a player's context is exactly the case the two failures are
    // kept apart for.
    CHECK(reasonOf("%basalt_player_name% = Steve") ==
          "%basalt_player_name% cannot be used here: there is no player here");
    const Who steve{.name = "Steve"};
    const PlaceholderContext context = PlaceholderContext{}.with(kWho, &steve);
    CHECK(PlaceholderCondition{"%basalt_player_name% = Steve"}.evaluate(registry, context).isTrue());

    CHECK(PlaceholderCondition{"%nothing% = 1"}.raw() == "%nothing% = 1");
}
