#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/LangTable.hpp>
#include <basalt/wire/text/Serializers.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <doctest.h>

#include <string>
#include <unordered_map>
#include <vector>

using namespace basalt::wire::text;
using basalt::wire::nbt::nbtIO::read;
using basalt::wire::nbt::nbtIO::write;

namespace {

Component roundTrip(const Component& component) {
    const auto bytes = write(componentNbt::serialize(component));
    const auto parsed = read(bytes);
    REQUIRE(parsed.has_value());
    return componentNbt::deserialize(parsed->tag);
}

} // namespace

TEST_CASE("components survive the NBT network form") {
    const Component simple = Component::text("hello", kRed).bold().underlined(false);
    CHECK(roundTrip(simple) == simple);

    const Component rich =
        Component::translatable("chat.type.text", {Component::text("Steve"), Component::text("hi")})
            .clickEvent(ClickEvent::openUrl("https://basaltmc.org"))
            .hoverEvent(
                HoverEvent{HoverEvent::ShowEntity{"minecraft:pig", "00000000-0000-0000-0000-000000000000", nullptr}})
            .append(Component::text(" more").italic());
    CHECK(roundTrip(rich) == rich);

    const Component selector{Component::Selector{"@a", std::make_shared<const Component>(Component::text(", "))}};
    CHECK(roundTrip(selector) == selector);

    const Component score = Component::score("Steve", "kills");
    CHECK(roundTrip(score) == score);

    const Component hoverText = Component::text("x").hoverEvent(HoverEvent::showText(Component::text("tip")));
    CHECK(roundTrip(hoverText) == hoverText);
}

TEST_CASE("translations resolve vanilla's format subset") {
    const LangTable table{{
        {"chat.type.text", "<%s> %s"},
        {"positional", "%2$s then %1$s"},
        {"percent", "100%% sure"},
        {"unsupported", "%d"},
    }};

    CHECK(resolvePlainText(Component::translatable("chat.type.text", {Component::text("Steve"), Component::text("hi")}),
                           table) == "<Steve> hi");
    CHECK(resolvePlainText(Component::translatable("positional", {Component::text("a"), Component::text("b")}),
                           table) == "b then a");
    CHECK(resolvePlainText(Component::translatable("percent"), table) == "100% sure");

    CHECK(resolvePlainText(Component::translatable("unsupported"), table) == "%d");
    CHECK(resolvePlainText(Component::translatable("chat.type.text", {Component::text("only one")}), table) ==
          "<%s> %s");
    CHECK(resolveTemplateStrict(Component::Translatable{"unsupported", {}, std::nullopt}, table).error() ==
          TranslationError::UnsupportedSpecifier);

    CHECK(resolvePlainText(Component::translatable("missing.key"), table) == "missing.key");
}

TEST_CASE("components survive the JSON form and its text") {
    const Component rich = Component::translatable("chat.type.text", {Component::text("Steve"), Component::text("hi")})
                               .color(kGold)
                               .italic(false)
                               .clickEvent(ClickEvent::suggestCommand("/msg Steve "))
                               .hoverEvent(HoverEvent::showText(Component::text("Click to reply")))
                               .append(Component::text("!"));

    const auto text = basalt::wire::common::writeJson(componentJson::serialize(rich));
    const auto parsed = basalt::wire::common::parseJson(text);
    REQUIRE(parsed.has_value());
    CHECK(componentJson::deserialize(*parsed) == rich);

    CHECK(componentJson::deserialize(basalt::wire::common::parseJson(R"("hi")").value()) == Component::text("hi"));
    CHECK(toPlainText(componentJson::deserialize(basalt::wire::common::parseJson(R"(["a","b"])").value())) == "ab");
}

TEST_CASE("the legacy serializer writes the codes a client without components reads") {
    using namespace basalt::wire::text;

    const Component line = Component::text("red and bold").color(kRed).bold(true);
    CHECK(toLegacyText(line) == "§c§lred and bold");

    SUBCASE("a child keeps what its parent styled and adds its own") {
        const Component parent =
            Component::text("grey ").color(kGray).append(Component::text("and yellow").color(kYellow));
        CHECK(toLegacyText(parent) == "§7grey §eand yellow");
    }

    SUBCASE("a colour the other side cannot draw becomes the nearest one it can") {
        CHECK(toLegacyText(Component::text("x").color(TextColor{0xAB0102})) == "§4x");
        CHECK(toLegacyText(Component::text("x").color(TextColor{0xFEFEFE})) == "§fx");
    }

    SUBCASE("plain text carries no codes at all") { CHECK(toLegacyText(Component::text("nothing")) == "nothing"); }
}
