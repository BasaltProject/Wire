#pragma once

#include <basalt/wire/wasm/Engine.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace basalt::wire::wasm {

/// A compiled and validated plugin module; copies share the code and are safe to share across threads.
class Module {
public:
    struct Impl;

    /// Compiles and checks the imports and exports against the ABI.
    [[nodiscard]] static Result<Module> compile(const Engine& engine, std::span<const std::uint8_t> wasm);

    [[nodiscard]] static Result<Module> deserialize(const Engine& engine, std::span<const std::uint8_t> compiled);

    /// Loads the compiled form from `cacheDir` when present and valid, else compiles and stores it there.
    [[nodiscard]] static Result<Module> compileCached(const Engine& engine, std::span<const std::uint8_t> wasm,
                                                      const std::filesystem::path& cacheDir);

    [[nodiscard]] static std::string cacheKey(const Engine& engine, std::span<const std::uint8_t> wasm);

    [[nodiscard]] Result<std::vector<std::uint8_t>> serialize() const;

    Module(const Module&) = default;
    Module(Module&&) noexcept = default;
    Module& operator=(const Module&) = default;
    Module& operator=(Module&&) noexcept = default;

private:
    explicit Module(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}

    std::shared_ptr<const Impl> impl_;
    friend class Instance;
};

} // namespace basalt::wire::wasm
