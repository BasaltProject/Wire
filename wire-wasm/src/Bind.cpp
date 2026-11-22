#include "Internal.hpp"

#include <basalt/wire/wasm/Abi.hpp>

#include <chrono>
#include <random>

namespace basalt::wire::wasm::detail {
namespace {
constexpr std::int32_t kWasiSuccess = 0;
constexpr std::int32_t kWasiBadFile = 8;
constexpr std::int32_t kWasiFault = 21;
constexpr std::int32_t kWasiInvalid = 28;

[[nodiscard]] std::uint64_t systemNanos(std::uint32_t clockId) {
    using namespace std::chrono;
    if (clockId == 0) {
        return static_cast<std::uint64_t>(duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count());
    }
    return static_cast<std::uint64_t>(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

void systemRandom(std::span<std::uint8_t> out) {
    std::random_device device;
    for (std::uint8_t& byte : out) {
        byte = static_cast<std::uint8_t>(device());
    }
}

void fdWrite(void* user, Call& call) {
    const WasiHooks& hooks = static_cast<WasiState*>(user)->hooks();
    const std::int32_t fd = call.i32(0);
    if (fd != 1 && fd != 2) {
        call.ret(kWasiBadFile);
        return;
    }
    const GuestMemory memory = call.memory();
    const std::int32_t iovs = call.i32(1);
    const std::int32_t count = call.i32(2);
    std::uint32_t written = 0;
    for (std::int32_t index = 0; index < count; ++index) {
        const auto base = memory.read<std::int32_t>(iovs + index * 8);
        const auto length = memory.read<std::int32_t>(iovs + index * 8 + 4);
        const auto text = base && length ? memory.string(*base, *length) : std::nullopt;
        if (!text) {
            call.ret(kWasiFault);
            return;
        }
        if (hooks.log != nullptr && !text->empty()) {
            hooks.log(hooks.user, fd, *text);
        }
        written += static_cast<std::uint32_t>(text->size());
    }
    call.ret(memory.write(call.i32(3), written) ? kWasiSuccess : kWasiFault);
}

void clockTimeGet(void* user, Call& call) {
    const WasiHooks& hooks = static_cast<WasiState*>(user)->hooks();
    const auto clockId = static_cast<std::uint32_t>(call.i32(0));
    if (clockId > 1) {
        call.ret(kWasiInvalid);
        return;
    }
    const std::uint64_t nanos =
        hooks.clockNanos != nullptr ? hooks.clockNanos(hooks.user, clockId) : systemNanos(clockId);
    call.ret(call.memory().write(call.i32(2), nanos) ? kWasiSuccess : kWasiFault);
}

void randomGet(void* user, Call& call) {
    const WasiHooks& hooks = static_cast<WasiState*>(user)->hooks();
    const auto view =
        call.memory().range(static_cast<std::uint32_t>(call.i32(0)), static_cast<std::uint32_t>(call.i32(1)));
    if (!view) {
        call.ret(kWasiFault);
        return;
    }
    if (hooks.randomBytes != nullptr) {
        hooks.randomBytes(hooks.user, *view);
    } else {
        systemRandom(*view);
    }
    call.ret(kWasiSuccess);
}

void emptyListSizes(void*, Call& call) {
    const GuestMemory memory = call.memory();
    const bool ok = memory.write(call.i32(0), std::uint32_t{0}) && memory.write(call.i32(1), std::uint32_t{0});
    call.ret(ok ? kWasiSuccess : kWasiFault);
}

void emptyList(void*, Call& call) { call.ret(kWasiSuccess); }

void procExit(void*, Call& call) { call.trap("proc_exit"); }

[[nodiscard]] HostFn wasiFunction(std::string_view name) {
    if (name == "fd_write") {
        return fdWrite;
    }
    if (name == "clock_time_get") {
        return clockTimeGet;
    }
    if (name == "random_get") {
        return randomGet;
    }
    if (name == "environ_sizes_get" || name == "args_sizes_get") {
        return emptyListSizes;
    }
    if (name == "environ_get" || name == "args_get") {
        return emptyList;
    }
    return procExit;
}
} // namespace

Result<std::vector<Binding>> buildBindings(const ModuleInfo& info, std::span<const HostImport> imports,
                                           WasiState& wasi) {
    const std::span<const AbiFunction> functions = abiFunctions();
    std::vector<Binding> bindings(functions.size());
    for (const HostImport& import : imports) {
        const std::optional<std::size_t> index = findAbiFunction(import.module, import.name);
        if (!index || import.fn == nullptr) {
            return fail(ErrorCode::Link, "host import '" + std::string{import.module} + "." + std::string{import.name} +
                                             "' is not part of the plugin ABI");
        }
        bindings[*index] = Binding{import.fn, import.user};
    }
    for (std::size_t index = 0; index < functions.size(); ++index) {
        if (functions[index].module == kWasiModule) {
            bindings[index] = Binding{wasiFunction(functions[index].name), &wasi};
        }
    }
    for (const ImportInfo& entry : info.imports) {
        const std::optional<std::size_t> index = findAbiFunction(entry.module, entry.name);
        if (!index || bindings[*index].fn == nullptr) {
            return fail(ErrorCode::Link, "the host provides no implementation of " + entry.module + "." + entry.name);
        }
    }
    return bindings;
}

} // namespace basalt::wire::wasm::detail
