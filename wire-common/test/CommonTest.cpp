#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/common/collections/PrimitiveMap.hpp>
#include <basalt/wire/common/hash/Md5.hpp>
#include <basalt/wire/common/json/Json.hpp>

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
    CHECK(hex(md5(std::string_view{"12345678901234567890123456789012345678901234567890123456789012345678901234567890"})) ==
          "57edf4a22be3c955ac49da2e2107b67a");
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
