#pragma once

#include <basalt/wire/wasm/Call.hpp>
#include <basalt/wire/wasm/Module.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace basalt::wire::wasm {

/// What the wasi subset needs from the host; a null member falls back to the system clock and random source.
struct WasiHooks {
    void* user = nullptr;
    void (*log)(void* user, std::int32_t fd, std::string_view text) = nullptr;
    std::uint64_t (*clockNanos)(void* user, std::uint32_t clockId) = nullptr;
    void (*randomBytes)(void* user, std::span<std::uint8_t> out) = nullptr;
};

struct InstanceOptions {
    std::span<const HostImport> imports;
    WasiHooks wasi;
};

/// One plugin's isolated state. Drive it from one thread at a time; only interrupt() may come from another.
class Instance {
public:
    struct Impl;

    /// Links every import the module declares, instantiates, and checks that basalt_abi_version returns 1.
    [[nodiscard]] static Result<Instance> create(const Module& module, const InstanceOptions& options);

    Instance(Instance&&) noexcept;
    Instance& operator=(Instance&&) noexcept;
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    ~Instance();

    [[nodiscard]] Result<void> init();

    [[nodiscard]] Result<void> callback(std::int32_t token, std::int32_t ctx);

    /// Traps the call in progress; a call that has not started yet is not affected. Callable from any thread.
    void interrupt() noexcept;

    [[nodiscard]] GuestMemory memory() const noexcept;

private:
    explicit Instance(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

} // namespace basalt::wire::wasm
