#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/plugins/PluginFolder.hpp>
#include <basalt/wire/plugins/PluginPackage.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace basalt::wire::plugins;
namespace fileIO = basalt::wire::io::fileIO;

namespace {
[[nodiscard]] std::string scratch() {
    const std::filesystem::path directory{BASALT_TEST_SCRATCH_DIR};
    return (directory / "oracle_plugins").string();
}

void write(const std::string& path, std::string_view text) {
    fileIO::createDirectories(path.substr(0, path.find_last_of('/')));
    fileIO::writeText(path, text);
}

[[nodiscard]] std::string bracketed(const std::vector<std::string>& values) {
    std::string text = "[";
    for (const std::string& value : values) {
        if (text.size() > 1) {
            text += ", ";
        }
        text += value;
    }
    return text + "]";
}

[[nodiscard]] std::string without(std::string_view path, std::string_view prefix) {
    return std::string{path.starts_with(prefix) ? path.substr(prefix.size()) : path};
}

constexpr std::string_view kManifestText = R"(id = "arena"
version = "1.0.0"
api = 1

[resources]
# A pattern with a leading slash, which is the same pattern.
private = ["/templates/", "seed.json"])";

class NotEditable final : public PluginPackage {
public:
    explicit NotEditable(const PluginPackage& inner) : PluginPackage(inner.origin()), inner_(&inner) {}

    [[nodiscard]] bool editable() const noexcept override { return false; }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::string_view path) const override {
        return inner_->read(path);
    }

    [[nodiscard]] std::vector<std::string> list(std::string_view prefix) const override { return inner_->list(prefix); }

private:
    const PluginPackage* inner_;
};
} // namespace

TEST_CASE("what a package puts in a plugin's folder, and what it leaves alone") {
    const std::string root = scratch();
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    write(root + "/arena/plugin.toml", kManifestText);
    write(root + "/arena/resources/config.toml", "balance = 100");
    write(root + "/arena/resources/lang/en_us.json", "{}");
    write(root + "/arena/resources/templates/map.json", "{}");
    write(root + "/arena/resources/seed.json", "{}");
    write(root + "/arena/resources/seed.json.bak", "{}");

    const FolderPackage folder{root + "/arena"};
    const std::expected<PluginManifest, ManifestError> manifest = PluginManifest::parse(kManifestText);
    REQUIRE(manifest.has_value());

    std::vector<std::string> written;
    PluginFolder{folder, *manifest, root + "/arena-out"}.materialize(
        [&](std::string_view line) { written.emplace_back(line); });
    CHECK(written.empty());

    const std::string live = root + "/live";
    write(live + "/config.toml", "balance = 999");
    const NotEditable packed{folder};
    std::vector<std::string> copied;
    PluginFolder{packed, *manifest, live}.materialize(
        [&](std::string_view line) { copied.push_back(without(line, "wrote " + live + "/")); });
    std::sort(copied.begin(), copied.end());
    CHECK(bracketed(copied) == "[lang/en_us.json, seed.json.bak]");
    // The file that was already there is untouched: an update never overwrites what was edited.
    CHECK(fileIO::readText(live + "/config.toml") == "balance = 999");

    const std::expected<std::map<std::string, std::string, std::less<>>, PluginLoadError> index =
        PluginFolder{folder, *manifest, live}.index();
    REQUIRE(index.has_value());
    std::vector<std::string> keys;
    for (const auto& [key, path] : *index) {
        keys.push_back(key);
    }
    CHECK(bracketed(keys) == "[config.toml, lang/en_us.json, seed.json.bak]");

    write(live + "/nested/config.toml", "clash = true");
    const std::expected<std::map<std::string, std::string, std::less<>>, PluginLoadError> clash =
        PluginFolder{folder, *manifest, live}.index();
    REQUIRE_FALSE(clash.has_value());
    CHECK(clash.error().message == "two files are called 'config.toml' (" + live + "/config.toml and " + live +
                                       "/nested/config.toml), and one name has to mean one file");

    fileIO::remove(live + "/nested/config.toml");
    write(live + "/nested/en_us.json", "{}");
    const std::expected<std::map<std::string, std::string, std::less<>>, PluginLoadError> jsonClash =
        PluginFolder{folder, *manifest, live}.index();
    REQUIRE_FALSE(jsonClash.has_value());
    CHECK(jsonClash.error().message == "two files are called 'en_us.json' (" + live + "/lang/en_us.json and " + live +
                                           "/nested/en_us.json), and one name has to mean one file");

    std::filesystem::remove_all(root, ignored);
}
