#include <basalt/wire/common/hash/Sha256.hpp>
#include <basalt/wire/wasm/Module.hpp>

#include <fstream>
#include <iterator>
#include <system_error>

namespace basalt::wire::wasm {
namespace {
[[nodiscard]] std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void storeFile(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!stream) {
            return;
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
    }
}
} // namespace

std::string Module::cacheKey(const Engine& engine, std::span<const std::uint8_t> wasm) {
    constexpr std::string_view kDigits = "0123456789abcdef";
    std::string key{engine.backend()};
    key += '-';
    for (const std::uint8_t byte : common::sha256(wasm)) {
        key += kDigits[byte >> 4];
        key += kDigits[byte & 0xF];
    }
    return key;
}

Result<Module> Module::compileCached(const Engine& engine, std::span<const std::uint8_t> wasm,
                                     const std::filesystem::path& cacheDir) {
    if (!engine.supportsSerialization()) {
        return compile(engine, wasm);
    }
    const std::filesystem::path file = cacheDir / (cacheKey(engine, wasm) + ".cwasm");
    std::error_code error;
    if (std::filesystem::is_regular_file(file, error)) {
        const std::vector<std::uint8_t> bytes = readFile(file);
        if (Result<Module> cached = deserialize(engine, bytes); cached) {
            return cached;
        }
    }
    Result<Module> module = compile(engine, wasm);
    if (!module) {
        return module;
    }
    if (Result<std::vector<std::uint8_t>> bytes = module->serialize(); bytes) {
        std::filesystem::create_directories(cacheDir, error);
        storeFile(file, *bytes);
    }
    return module;
}

} // namespace basalt::wire::wasm
