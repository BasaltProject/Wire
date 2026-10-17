#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/plugins/PluginSignature.hpp>

#include <doctest.h>

#include <filesystem>
#include <string>

using namespace basalt::wire::plugins;
namespace fileIO = basalt::wire::io::fileIO;

TEST_CASE("bytecode loads only with a signature from a trusted key that still matches it") {
    const std::string root = (std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / "signed_plugin").string();
    std::filesystem::remove_all(root);
    fileIO::createDirectories(root + "/src");
    fileIO::writeText(root + "/plugin.toml", "id = \"demo\"\n");
    fileIO::writeText(root + "/src/main.luac", "\x1bLua-one");

    const signature::Seed seed{1, 2, 3};
    const signature::PublicKey key = signature::publicKeyOf(seed);
    const FolderPackage folder{root};
    CHECK(signature::hasBytecode(folder));
    fileIO::writeText(root + "/plugin.sig", signature::render(key, signature::sign(seed, signature::payload(folder))));

    const signature::PublicKey stranger = signature::publicKeyOf(signature::Seed{9});
    CHECK(signature::verify(folder, std::span{&key, 1}).value() == key);
    CHECK_FALSE(signature::verify(folder, std::span{&stranger, 1}).has_value());

    fileIO::writeText(root + "/src/main.luac", "\x1bLua-two");
    CHECK_FALSE(signature::verify(folder, std::span{&key, 1}).has_value());
}
