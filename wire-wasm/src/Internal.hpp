#pragma once

#include <basalt/wire/wasm/Abi.hpp>
#include <basalt/wire/wasm/Call.hpp>
#include <basalt/wire/wasm/Error.hpp>
#include <basalt/wire/wasm/Instance.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace basalt::wire::wasm::detail {

struct ImportInfo {
    std::string module;
    std::string name;
    char kind = '?';
    std::string signature;
};

struct ExportInfo {
    std::string name;
    char kind = '?';
    std::string signature;
};

struct ModuleInfo {
    std::vector<ImportInfo> imports;
    std::vector<ExportInfo> exports;
};

struct Binding {
    HostFn fn = nullptr;
    void* user = nullptr;
};

class WasiState {
public:
    explicit WasiState(const WasiHooks& hooks) : hooks_(hooks) {}

    [[nodiscard]] const WasiHooks& hooks() const noexcept { return hooks_; }

private:
    WasiHooks hooks_;
};

/// The imports parsed from the ABI headers at configure time (AbiGenerated.cpp in the build directory).
[[nodiscard]] std::span<const AbiFunction> generatedAbiFunctions();

[[nodiscard]] Result<void> validateModule(const ModuleInfo& info, Profile profile);

/// One binding per entry of abiFunctions(), host functions for the basalt module and ours for wasi.
[[nodiscard]] Result<std::vector<Binding>> buildBindings(const ModuleInfo& info, std::span<const HostImport> imports,
                                                         WasiState& wasi);

} // namespace basalt::wire::wasm::detail
