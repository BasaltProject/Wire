#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/LangTable.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/Serializers.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <string>
#include <unordered_map>
#include <vector>

using namespace basalt::wire::text;
using basalt::wire::nbt::nbtIO::read;
using basalt::wire::nbt::nbtIO::write;

namespace {

/// Through the tag tree and through the bytes, so a component that survives here also survives the
/// network path it actually travels on.
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
            .hoverEvent(HoverEvent{HoverEvent::ShowEntity{"minecraft:pig", "00000000-0000-0000-0000-000000000000",
                                                          nullptr}})
            .append(Component::text(" more").italic());
    CHECK(roundTrip(rich) == rich);

    const Component selector{
        Component::Selector{"@a", std::make_shared<const Component>(Component::text(", "))}};
    CHECK(roundTrip(selector) == selector);

    const Component score = Component::score("Steve", "kills");
    CHECK(roundTrip(score) == score);

    const Component hoverText = Component::text("x").hoverEvent(HoverEvent::showText(Component::text("tip")));
    CHECK(roundTrip(hoverText) == hoverText);
}

TEST_CASE("style inheritance is tri-state, not on or off") {
    Style parent;
    parent.bold = true;
    parent.italic = true;
    Style child;
    child.italic = false;

    const Style merged = parent.merge(child);
    CHECK(merged.bold == true);
    CHECK(merged.italic == false);
    CHECK_FALSE(merged.color.has_value());
}

TEST_CASE("MiniMessage builds the component tree the markup describes") {
    const Component parsed = miniMessage::parse("<red>hello <bold>world</bold></red>!");
    CHECK(toPlainText(parsed) == "hello world!");

    const Component colored = miniMessage::parse("<#ff8800>x");
    CHECK(colored.style().color == TextColor{0xFF8800});

    const Component clickable = miniMessage::parse("<click:run_command:'/say hi'>go");
    REQUIRE(clickable.style().clickEvent.has_value());
    CHECK(clickable.style().clickEvent->action == ClickEvent::Action::RunCommand);
    CHECK(clickable.style().clickEvent->value == "/say hi");

    // An escaped angle bracket is literal text, not the start of a tag.
    CHECK(toPlainText(miniMessage::parse("\\<red>")) == "<red>");

    const Component gradient = miniMessage::parse("<gradient:red:blue>abc</gradient>");
    CHECK(toPlainText(gradient) == "abc");
    REQUIRE(gradient.children().size() == 3);
    CHECK(gradient.children().front().style().color == kRed);
    CHECK(gradient.children().back().style().color == kBlue);
}

TEST_CASE("translations resolve vanilla's format subset") {
    const LangTable table{{
        {"chat.type.text", "<%s> %s"},
        {"positional", "%2$s then %1$s"},
        {"percent", "100%% sure"},
        {"unsupported", "%d"},
    }};

    CHECK(resolvePlainText(Component::translatable("chat.type.text",
                                                   {Component::text("Steve"), Component::text("hi")}),
                           table) == "<Steve> hi");
    CHECK(resolvePlainText(Component::translatable("positional", {Component::text("a"), Component::text("b")}),
                           table) == "b then a");
    CHECK(resolvePlainText(Component::translatable("percent"), table) == "100% sure");

    // An unsupported specifier and a missing argument both fall back to the raw format string,
    // because a bad message must not take down whatever was printing it.
    CHECK(resolvePlainText(Component::translatable("unsupported"), table) == "%d");
    CHECK(resolvePlainText(Component::translatable("chat.type.text", {Component::text("only one")}), table) ==
          "<%s> %s");
    CHECK(resolveTemplateStrict(Component::Translatable{"unsupported", {}, std::nullopt}, table).error() ==
          TranslationError::UnsupportedSpecifier);

    // A key with no entry renders as itself, or as its fallback when it has one.
    CHECK(resolvePlainText(Component::translatable("missing.key"), table) == "missing.key");
}

TEST_CASE("ANSI output only resets when it actually styled something") {
    CHECK(toAnsi(Component::text("plain")) == "plain");
    const std::string styled = toAnsi(Component::text("x", kRed));
    CHECK(styled.starts_with("\x1B[38;2;255;85;85m"));
    CHECK(styled.ends_with("\x1B[0m"));
}

TEST_CASE("components survive the JSON form and its text") {
    const Component rich =
        Component::translatable("chat.type.text", {Component::text("Steve"), Component::text("hi")})
            .color(kGold)
            .italic(false)
            .clickEvent(ClickEvent::suggestCommand("/msg Steve "))
            .hoverEvent(HoverEvent::showText(Component::text("Click to reply")))
            .append(Component::text("!"));

    const auto text = basalt::wire::common::writeJson(componentJson::serialize(rich));
    const auto parsed = basalt::wire::common::parseJson(text);
    REQUIRE(parsed.has_value());
    CHECK(componentJson::deserialize(*parsed) == rich);

    // A bare string is a component, and so is an array of them.
    CHECK(componentJson::deserialize(basalt::wire::common::parseJson(R"("hi")").value()) == Component::text("hi"));
    CHECK(toPlainText(componentJson::deserialize(basalt::wire::common::parseJson(R"(["a","b"])").value())) == "ab");
}

TEST_CASE("a language file parses into a lookup table") {
    const auto table = parseLangTable(R"({"a.b":"Hello %s","ignored":42})");
    REQUIRE(table.has_value());
    CHECK(resolvePlainText(Component::translatable("a.b", {Component::text("world")}), *table) == "Hello world");
    CHECK(parseLangTable("[]").error() == basalt::wire::common::JsonError::WrongType);
}
