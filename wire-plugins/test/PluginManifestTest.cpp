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
