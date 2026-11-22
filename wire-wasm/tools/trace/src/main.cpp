#include "HostApi.hpp"
#include "Scenario.hpp"

#include <basalt/wire/plugins/PluginManifest.hpp>
#include <basalt/wire/wasm/Instance.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

namespace basalt::wire::wasm::trace {
namespace {
constexpr std::uint64_t kEpochNanos = 1'700'000'000'000'000'000ULL;
constexpr std::uint64_t kNanosPerTick = 50'000'000ULL;

struct FakeSystem {
    World* world = nullptr;
    std::uint64_t random = 0x9E3779B97F4A7C15ULL;
};

[[nodiscard]] bool readFile(const std::filesystem::path& path, std::string& out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return true;
}

void wasiLog(void*, std::int32_t fd, std::string_view text) {
    emitLine("wasi fd=" + std::to_string(fd) + " " + quote(text));
}

std::uint64_t wasiClock(void* user, std::uint32_t) {
    const auto* system = static_cast<FakeSystem*>(user);
    return kEpochNanos + static_cast<std::uint64_t>(system->world->now) * kNanosPerTick;
}

void wasiRandom(void* user, std::span<std::uint8_t> out) {
    auto* system = static_cast<FakeSystem*>(user);
    for (std::uint8_t& byte : out) {
        system->random ^= system->random << 13;
        system->random ^= system->random >> 7;
        system->random ^= system->random << 17;
        byte = static_cast<std::uint8_t>(system->random);
    }
}

[[nodiscard]] bool loadTable(const std::filesystem::path& path, common::toml::TomlTable& out) {
    std::string text;
    if (!readFile(path, text)) {
        return true;
    }
    auto parsed = common::toml::parse(text);
    if (!parsed) {
        std::fprintf(stderr, "%s: %s\n", path.string().c_str(),
                     std::string{common::toml::tomlErrorMessage(parsed.error())}.c_str());
        return false;
    }
    out = std::move(*parsed);
    return true;
}

int run(const std::filesystem::path& pluginDir, std::istream& scenario) {
    std::string manifestText;
    if (!readFile(pluginDir / "plugin.toml", manifestText)) {
        std::fprintf(stderr, "no plugin.toml in %s\n", pluginDir.string().c_str());
        return 2;
    }
    const auto manifest = plugins::PluginManifest::parse(manifestText);
    if (!manifest) {
        std::fprintf(stderr, "plugin.toml: %s\n", manifest.error().message.c_str());
        return 2;
    }
    const auto raw = common::toml::parse(manifestText);
    const std::int64_t abi = raw->integer("abi").value_or(0);
    if (abi != kAbiVersion) {
        std::fprintf(stderr, "plugin.toml asks for abi %lld, this tool speaks %d\n", static_cast<long long>(abi),
                     kAbiVersion);
        return 2;
    }

    World world;
    world.playersDataGranted = manifest->capabilities.playersData;
    world.capabilities = manifest->capabilities;
    world.exportedServices = manifest->exports.services;
    if (!loadTable(pluginDir / "resources" / "config.toml", world.config) ||
        !loadTable(pluginDir / "resources" / "lang" / "en_us.toml", world.lang)) {
        return 2;
    }

    std::string wasmText;
    if (!readFile(pluginDir / manifest->main, wasmText)) {
        std::fprintf(stderr, "cannot read %s\n", (pluginDir / manifest->main).string().c_str());
        return 2;
    }
    const std::vector<std::uint8_t> wasm(wasmText.begin(), wasmText.end());

    auto engine = Engine::create();
    if (!engine) {
        std::fprintf(stderr, "engine: %s\n", engine.error().message.c_str());
        return 3;
    }
    auto module = Module::compile(*engine, wasm);
    if (!module) {
        std::fprintf(stderr, "module: %s\n", module.error().message.c_str());
        return 3;
    }
    FakeSystem system{&world};
    const std::vector<HostImport> imports = buildImports(world);
    auto instance =
        Instance::create(*module, InstanceOptions{imports, WasiHooks{&system, wasiLog, wasiClock, wasiRandom}});
    if (!instance) {
        std::fprintf(stderr, "instance: %s\n", instance.error().message.c_str());
        return 3;
    }
    world.instance = &*instance;
    const bool ok = runScenario(world, scenario);
    std::fflush(stdout);
    return ok ? 0 : 2;
}
} // namespace
} // namespace basalt::wire::wasm::trace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: basalt-wasm-trace <plugin-dir> <scenario-file|->\n");
        return 64;
    }
    const std::string scenarioPath = argv[2];
    if (scenarioPath == "-") {
        return basalt::wire::wasm::trace::run(argv[1], std::cin);
    }
    std::ifstream file(scenarioPath);
    if (!file) {
        std::fprintf(stderr, "cannot read %s\n", scenarioPath.c_str());
        return 64;
    }
    return basalt::wire::wasm::trace::run(argv[1], file);
}
