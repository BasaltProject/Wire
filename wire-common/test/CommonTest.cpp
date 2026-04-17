#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/JavaNumbers.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/common/collections/PrimitiveMap.hpp>
#include <basalt/wire/common/hash/Md5.hpp>
#include <basalt/wire/common/hash/Sha256.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/common/timings/TickDurations.hpp>
#include <basalt/wire/common/timings/Timings.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::wire::common;

namespace {

std::string hex(std::span<const std::uint8_t> bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes) {
        out += kDigits[byte >> 4];
        out += kDigits[byte & 0x0F];
    }
    return out;
}

} // namespace

TEST_CASE("md5 matches the RFC 1321 test vectors") {
    CHECK(hex(md5(std::string_view{""})) == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(hex(md5(std::string_view{"a"})) == "0cc175b9c0f1b6a831c399e269772661");
    CHECK(hex(md5(std::string_view{"abc"})) == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(hex(md5(std::string_view{"message digest"})) == "f96b697d7cb7938d525a2f31aaf161d0");
    CHECK(hex(md5(std::string_view{"abcdefghijklmnopqrstuvwxyz"})) == "c3fcd3d76192e4007dfb496cca67e13b");
    CHECK(hex(md5(
              std::string_view{"12345678901234567890123456789012345678901234567890123456789012345678901234567890"})) ==
          "57edf4a22be3c955ac49da2e2107b67a");
}

TEST_CASE("sha-256 matches the FIPS 180-4 test vectors") {
    CHECK(hex(sha256(std::string_view{""})) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(hex(sha256(std::string_view{"abc"})) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    // Exactly one block short of needing a second one, which is where the padding goes wrong first.
    CHECK(hex(sha256(std::string_view{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"})) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(hex(sha256(std::string_view{
              "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrs"
              "mnopqrstnopqrstu"})) == "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
}

TEST_CASE("sha-256 pads a message that fills its last block exactly") {
    // 55 bytes leave one for the 0x80 and eight for the length; 56 pushes the length into a block
    // of its own, which is the case a hand-written padding loop gets wrong.
    CHECK(hex(sha256(std::string(55, 'a'))) == "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    CHECK(hex(sha256(std::string(56, 'a'))) == "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    CHECK(hex(sha256(std::string(64, 'a'))) == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
}

TEST_CASE("a random uuid is a version four uuid") {
    const basalt::wire::common::Uuid first = basalt::wire::common::Uuid::random();
    const basalt::wire::common::Uuid second = basalt::wire::common::Uuid::random();
    // parity: java.util.UUID.randomUUID stamps the version nibble and the two variant bits, which
    // is what makes the string form say `-4` in the third group and start the fourth with 8 to b.
    CHECK(((first.high() >> 12) & 0xF) == 4);
    CHECK(((first.low() >> 62) & 0x3) == 2);
    CHECK(first.toString()[14] == '4');
    const char variant = first.toString()[19];
    CHECK((variant == '8' || variant == '9' || variant == 'a' || variant == 'b'));

    CHECK_FALSE(first.isNil());
    CHECK(first != second);
    // And it round trips like any other.
    CHECK(basalt::wire::common::Uuid::parse(first.toString()) == first);
}

TEST_CASE("Identifier parses the vanilla resource id forms") {
    const auto bare = Identifier::parse("stone");
    REQUIRE(bare.has_value());
    CHECK(bare->namespaceName() == "minecraft");
    CHECK(bare->path() == "stone");

    const auto full = Identifier::parse("basalt:thing");
    REQUIRE(full.has_value());
    CHECK(full->toString() == "basalt:thing");

    CHECK(Identifier::parse("a:b:c").error() == IdentifierError::TooManySeparators);
    CHECK(Identifier::parse(":path").error() == IdentifierError::EmptyPart);
    CHECK(Identifier::parse("namespace:").error() == IdentifierError::EmptyPart);
}

TEST_CASE("Mth mirrors the vanilla sine table rather than the platform trigonometry") {
    CHECK(mth::sin(0.0) == 0.0F);
    CHECK(mth::cos(0.0) == doctest::Approx(1.0F).epsilon(1e-4));
    CHECK(mth::floor(-0.5) == -1);
    CHECK(mth::lfloor(-0.5) == -1);
    CHECK(mth::clampedLerp(-1.0, 2.0, 5.0) == 2.0);
    CHECK(mth::clampedLerp(2.0, 2.0, 5.0) == 5.0);
    CHECK(mth::fastInvSqrt(4.0) == doctest::Approx(0.5).epsilon(1e-3));
}

TEST_CASE("PrimitiveMap survives collisions, tombstones and growth") {
    PrimitiveMap<std::int64_t, int> map(4);
    for (int i = 0; i < 1000; ++i) {
        CHECK_FALSE(map.put(i, i * 2).has_value());
    }
    CHECK(map.size() == 1000);
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(map.find(i) != nullptr);
        CHECK(*map.find(i) == i * 2);
    }

    CHECK(map.put(500, 7).value() == 1000);
    CHECK(map.remove(500).value() == 7);
    CHECK_FALSE(map.contains(500));
    CHECK(map.size() == 999);
    CHECK(map.getOrDefault(500, -1) == -1);

    std::size_t seen = 0;
    map.forEach([&seen](std::int64_t, int) { ++seen; });
    CHECK(seen == 999);
}

TEST_CASE("JSON round trips the shapes datapacks and components use") {
    const auto parsed = parseJson(R"({"a":1,"b":[true,null,"x"],"c":{"d":1.5},"e":"line\nbreak"})");
    REQUIRE(parsed.has_value());
    const auto* root = parsed->get<JsonObject>();
    REQUIRE(root != nullptr);

    // An integer stays an integer: the datapack codecs care, and a double loses that.
    REQUIRE(root->find("a") != nullptr);
    CHECK(*root->find("a")->get<std::int64_t>() == 1);
    CHECK(root->find("c")->get<JsonObject>()->find("d")->get<double>() != nullptr);

    const auto* array = root->find("b")->get<std::vector<JsonValue>>();
    REQUIRE(array != nullptr);
    CHECK(array->size() == 3);
    CHECK(*(*array)[0].get<bool>());
    CHECK((*array)[1].isNull());

    const std::string written = writeJson(*parsed);
    const auto reparsed = parseJson(written);
    REQUIRE(reparsed.has_value());
    CHECK(*reparsed == *parsed);
    // Member order survives the round trip, so a file rewritten by the server still reads the same.
    CHECK(reparsed->get<JsonObject>()->names() == root->names());
}

TEST_CASE("malformed JSON is refused rather than thrown") {
    CHECK(parseJson("{").error() == JsonError::SyntaxError);
    CHECK(parseJson("").error() == JsonError::SyntaxError);
    CHECK_FALSE(parseJson(std::string(kJsonMaxDepth + 8, '[')).has_value());
}

TEST_CASE("TOML reads the shapes the config files use") {
    const auto parsed = toml::parse(R"(
# a leading comment
name = "Basalt"          # a trailing one
port = 25565
rate = 0.25
online = true
literal = 'no \escapes here'
escaped = "line\nbreak"

[network]
compression-threshold = 256
"quoted key" = 1

[network.limits]
max-players = 20

worlds = [
    "overworld",
    "nether",   # a comment inside an array
    "the_end",
]
)");
    REQUIRE(parsed.has_value());

    CHECK(parsed->string("name") == "Basalt");
    CHECK(parsed->integer("port") == 25565);
    CHECK(parsed->real("rate") == 0.25);
    // An integer where a float is expected still reads: a hand-edited file writes `1`, not `1.0`.
    CHECK(parsed->real("port") == 25565.0);
    CHECK(parsed->boolean("online") == true);
    CHECK(parsed->string("literal") == "no \\escapes here");
    CHECK(parsed->string("escaped") == "line\nbreak");

    const toml::TomlTable* network = parsed->table("network");
    REQUIRE(network != nullptr);
    CHECK(network->integer("compression-threshold") == 256);
    CHECK(network->integer("quoted key") == 1);

    const toml::TomlTable* limits = network->table("limits");
    REQUIRE(limits != nullptr);
    CHECK(limits->integer("max-players") == 20);
    // An array written one entry per line, with a comment and a trailing comma, is the readable way
    // to write it and has to survive.
    CHECK(limits->stringList("worlds") == std::vector<std::string>{"overworld", "nether", "the_end"});

    // Keys come back in the order they appear on the page, because that is the order they are
    // written back in.
    CHECK(parsed->keys().front() == "name");
}

TEST_CASE("TOML says no rather than guessing") {
    CHECK(toml::parse("online-mode = true\nonline-mode = false").error() == toml::TomlError::DuplicateKey);
    CHECK(toml::parse("network = false\n[network]\nport = 1").error() == toml::TomlError::ConflictingTable);
    CHECK(toml::parse("[network]\nport = 1\n[network]\nport = 2").error() == toml::TomlError::DuplicateKey);
    CHECK(toml::parse("[]").error() == toml::TomlError::MalformedTableHeader);
    CHECK(toml::parse("[[guides]]\nname = 'x'").error() == toml::TomlError::ArrayOfTablesUnsupported);
    CHECK(toml::parse("[unterminated").error() == toml::TomlError::MalformedTableHeader);
    CHECK(toml::parse("no separator here").error() == toml::TomlError::MissingSeparator);
    CHECK(toml::parse("x = \"unterminated").error() == toml::TomlError::UnterminatedString);
    CHECK(toml::parse("x = [1, 2").error() == toml::TomlError::UnterminatedArray);
    CHECK(toml::parse("x = \"bad \\q escape\"").error() == toml::TomlError::UnknownEscape);
    CHECK(toml::parse("x = notavalue").error() == toml::TomlError::NotAValue);
    // A `#` inside a string is part of the string, not the start of a comment.
    CHECK(toml::parse("x = \"a # b\"")->string("x") == "a # b");
}

TEST_CASE("tick durations report the outliers a TPS number hides") {
    TickDurations durations{64};
    const std::int64_t now = 100 * TickDurations::kNanosPerSecond;

    // Fifty ticks over the last second: forty-nine fast ones and one that stalled.
    for (int i = 0; i < 49; ++i) {
        durations.record(now - (49 - i) * 10'000'000, 1'000'000);
    }
    durations.record(now, 80'000'000);

    const DurationSummary summary = durations.summary(now, 5);
    CHECK(summary.samples == 50);
    CHECK(summary.minMillis == 1.0);
    CHECK(summary.medianMillis == 1.0);
    // The maximum is what says the server stuttered; the mean barely moves.
    CHECK(summary.maxMillis == 80.0);
    CHECK(summary.meanMillis < 3.0);

    // A server younger than the window is measured over the time it has actually existed, not over
    // the window, or a perfectly healthy young server reports a fraction of a tick per second.
    CHECK(durations.ticksPerSecond(now, 300) > 19.0);
    CHECK(durations.summary(now, 5).samples == 50);

    durations.reset();
    CHECK(durations.summary(now, 5) == DurationSummary{});
    CHECK(durations.ticksPerSecond(now, 5) == 0.0);
}

TEST_CASE("a disabled timing measures nothing and still runs the work") {
    Timings timings;
    Timing& section = timings.of("player.tick");
    CHECK(&timings.of("player.tick") == &section);

    int ran = 0;
    section.time([&ran] { ++ran; });
    CHECK(ran == 1);
    CHECK(section.snapshot().calls == 0);

    timings.setEnabled(true);
    section.time([&ran] { ++ran; });
    section.time([&ran] { ++ran; });
    CHECK(ran == 3);

    const TimingSnapshot snapshot = section.snapshot();
    CHECK(snapshot.calls == 2);
    CHECK(snapshot.maxNanos >= snapshot.averageNanos());
    CHECK(timings.snapshot().size() == 1);
}

TEST_CASE("numbers print the way Java's toString prints them") {
    using basalt::wire::common::javaToString;
    // Vectors from a JDK 25 run of Double.toString and Float.toString.
    CHECK(javaToString(70.0) == "70.0");
    CHECK(javaToString(0.001) == "0.001");
    CHECK(javaToString(1.0E7) == "1.0E7");
    CHECK(javaToString(9999999.0) == "9999999.0");
    CHECK(javaToString(1.0E-4) == "1.0E-4");
    CHECK(javaToString(123456.789) == "123456.789");
    CHECK(javaToString(-0.5) == "-0.5");
    CHECK(javaToString(1.2345E-5) == "1.2345E-5");
    CHECK(javaToString(0.1 + 0.2) == "0.30000000000000004");
    CHECK(javaToString(1e21) == "1.0E21");
    CHECK(javaToString(100.0) == "100.0");
    CHECK(javaToString(17.12F) == "17.12");
    CHECK(javaToString(0.1F) == "0.1");
    CHECK(javaToString(1e10F) == "1.0E10");
    CHECK(javaToString(19.0F) == "19.0");
    CHECK(javaToString(0.00099F) == "9.9E-4");
    CHECK(javaToString(16777216.0F) == "1.6777216E7");
}
