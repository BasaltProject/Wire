#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/plugins/PluginManifest.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::wire::plugins;

TEST_CASE("versions compare the way the frozen kotlin compares them") {
    CHECK(compareVersions("1.0.0", "1.0.0") == 0);
    CHECK(compareVersions("1.0", "1.0.0") == 0);
    CHECK(compareVersions("1.2.3", "1.2.4") == -1);
    CHECK(compareVersions("1.10.0", "1.9.0") == 1);
    CHECK(compareVersions("2.0", "1.99.99") == 1);
    CHECK(compareVersions("1.0.0-beta", "1.0.0") == 0);
    CHECK(compareVersions("1.0.0-2", "1.0.0-10") == -1);
    CHECK(compareVersions("", "0") == 0);
    CHECK(compareVersions("1.0.0", "1.0.0.0") == 0);
}

TEST_CASE("a requirement accepts what the frozen kotlin accepts") {
    static constexpr std::string_view kCandidates[] = {"1.2.3", "1.2.0", "2.0.0", "0.9"};
    const auto accepts = [](std::string_view requirement) {
        std::string answers;
        for (const std::string_view candidate : kCandidates) {
            if (!answers.empty()) {
                answers += ',';
            }
            answers +=
                PluginDependency{.id = "x", .requirement = std::string{requirement}, .hard = true}.accepts(candidate)
                    ? "true"
                    : "false";
        }
        return answers;
    };

    CHECK(accepts("*") == "true,true,true,true");
    CHECK(accepts("1.2.3") == "true,false,false,false");
    CHECK(accepts(">=1.2") == "true,true,true,false");
    CHECK(accepts("> 1.2") == "true,false,true,false");
    CHECK(accepts("<=2.0") == "true,true,true,true");
    CHECK(accepts("<2.0") == "true,true,false,true");
    CHECK(accepts("  1.2.3  ") == "true,false,false,false");
}

TEST_CASE("a webassembly plugin must name the abi it was built against") {
    const auto parse = [](std::string_view abi) {
        return PluginManifest::parse("id = \"wasmy\"\nversion = \"1\"\napi = 1\nmain = \"plugin.wasm\"\n" +
                                     std::string{abi});
    };
    CHECK_FALSE(parse("").has_value());
    CHECK_FALSE(parse("abi = 2\n").has_value());
    const auto ok = parse("abi = 1\n");
    REQUIRE(ok.has_value());
    CHECK(ok->isWasm());
    CHECK(ok->abi == 1);
}

TEST_CASE("capabilities are a closed set of known names") {
    const auto parse = [](std::string_view body) {
        return PluginManifest::parse("id = \"wasmy\"\nversion = \"1\"\napi = 1\nabi = 1\nmain = \"plugin.wasm\"\n"
                                     "[capabilities]\n" +
                                     std::string{body});
    };
    const auto ok = parse("players_data = true\nworldgen = true\nnetwork = [\"a:b\"]\ncommands = false\n");
    REQUIRE(ok.has_value());
    CHECK(ok->capabilities.has("players_data"));
    CHECK(ok->capabilities.has("worldgen"));
    CHECK(ok->capabilities.has("network"));
    CHECK_FALSE(ok->capabilities.has("commands"));
    CHECK_FALSE(parse("teleport = true\n").has_value());
    CHECK_FALSE(parse("network = true\n").has_value());
    CHECK_FALSE(parse("storage = \"yes\"\n").has_value());
}
