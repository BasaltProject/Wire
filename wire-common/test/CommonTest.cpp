#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/JavaNumbers.hpp>
#include <basalt/wire/common/hash/Md5.hpp>
#include <basalt/wire/common/hash/Sha256.hpp>
#include <basalt/wire/common/json/Json.hpp>
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
    CHECK(hex(sha256(std::string_view{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"})) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(hex(sha256(std::string_view{
              "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrs"
              "mnopqrstnopqrstu"})) == "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
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

TEST_CASE("JSON round trips the shapes datapacks and components use") {
    const auto parsed = parseJson(R"({"a":1,"b":[true,null,"x"],"c":{"d":1.5},"e":"line\nbreak"})");
    REQUIRE(parsed.has_value());
    const auto* root = parsed->get<JsonObject>();
    REQUIRE(root != nullptr);

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
    CHECK(reparsed->get<JsonObject>()->names() == root->names());
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
    CHECK(limits->stringList("worlds") == std::vector<std::string>{"overworld", "nether", "the_end"});

    CHECK(parsed->keys().front() == "name");
}

TEST_CASE("numbers print the way Java's toString prints them") {
    using basalt::wire::common::javaToString;
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
