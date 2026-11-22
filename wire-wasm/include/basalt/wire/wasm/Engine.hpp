#pragma once

#include <basalt/wire/wasm/Abi.hpp>
#include <basalt/wire/wasm/Error.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace basalt::wire::wasm {

struct EngineLimits {
    std::uint64_t maxMemoryBytes = 64ULL * 1024 * 1024;
    std::size_t maxStackBytes = 512 * 1024;
    Profile profile = Profile::Server;
};

/// Shared by every module and instance made from it; safe to use from any thread.
class Engine {
public:
    struct Impl;

    [[nodiscard]] static Result<Engine> create(const EngineLimits& limits = {});

    Engine(const Engine&) = default;
    Engine(Engine&&) noexcept = default;
    Engine& operator=(const Engine&) = default;
    Engine& operator=(Engine&&) noexcept = default;

    [[nodiscard]] const EngineLimits& limits() const noexcept;

    /// "wasmtime-51.0.0" or "wamr-2.4.6": part of every compiled-module cache key.
    [[nodiscard]] std::string_view backend() const noexcept;

    [[nodiscard]] bool supportsSerialization() const noexcept;

private:
    explicit Engine(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}

    std::shared_ptr<Impl> impl_;
    friend class Module;
    friend class Instance;
};

} // namespace basalt::wire::wasm
