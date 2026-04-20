#include <basalt/wire/io/ConfigFiles.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>

using namespace basalt::wire::io;

namespace {
struct ConfigFixture {
    std::filesystem::path root;

    ConfigFixture() {
        static std::atomic<unsigned> sequence{0};
        root = std::filesystem::temp_directory_path() /
               ("basalt-config-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                std::to_string(sequence.fetch_add(1)));
    }

    ~ConfigFixture() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
};
} // namespace

TEST_CASE("shared configuration generates once, preserves edits and reports the responsible file") {
    ConfigFixture fixture;
    const auto root = fixture.root.string();
    auto generated = configFiles::loadToml(root, "test.toml", "# Commented default\nmode = true\n");
    REQUIRE(generated);
    CHECK(generated->generated);
    REQUIRE(fileIO::writeText(root + "/test.toml", "mode = false\n"));
    auto edited = configFiles::loadToml(root, "test.toml", "mode = true\n");
    REQUIRE(edited);
    CHECK_FALSE(edited->generated);
    CHECK(edited->values.boolean("mode") == false);
    REQUIRE(fileIO::writeText(root + "/test.toml", "[broken"));
    auto broken = configFiles::loadToml(root, "test.toml", "mode = true\n");
    REQUIRE_FALSE(broken);
    CHECK(broken.error().starts_with(root + "/test.toml is not valid TOML:"));
    CHECK(fileIO::readText(root + "/test.toml") == "[broken");
}
