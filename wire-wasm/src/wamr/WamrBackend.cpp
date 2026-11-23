#include "../Internal.hpp"

#include <basalt/wire/wasm/Abi.hpp>

#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>
#include <wasm_export.h>

#ifndef BASALT_WASM_BACKEND_ID
#error "the build must define BASALT_WASM_BACKEND_ID"
#endif

namespace basalt::wire::wasm {
namespace {
constexpr std::size_t kMaxFunctions = 1024;
constexpr std::size_t kSlotBytes = sizeof(std::uint64_t);
constexpr std::uint32_t kWasmPageBytes = 65536;
constexpr std::size_t kErrorBytes = 256;
} // namespace

namespace {
template <std::size_t I>
void rawTrampoline(wasm_exec_env_t env, std::uint64_t* args);

class Runtime {
public:
    [[nodiscard]] static bool acquire();
    static void release();
};

std::mutex gRuntimeMutex;
std::size_t gRuntimeUsers = 0;
std::thread::id gInitThread;
std::deque<std::string> gSignatures;
std::array<NativeSymbol, kMaxFunctions> gBasaltSymbols;
std::array<NativeSymbol, kMaxFunctions> gWasiSymbols;
std::array<NativeSymbol, kMaxFunctions> gProxySymbols;
std::size_t gBasaltCount = 0;
std::size_t gWasiCount = 0;
std::size_t gProxyCount = 0;
} // namespace

struct Engine::Impl {
    EngineLimits limits;
    bool holdsRuntime = false;

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ~Impl() {
        if (holdsRuntime) {
            Runtime::release();
        }
    }
};

struct Module::Impl {
    std::shared_ptr<Engine::Impl> engine;
    std::vector<std::uint8_t> bytes;
    wasm_module_t module = nullptr;
    detail::ModuleInfo info;

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ~Impl() {
        if (module != nullptr) {
            wasm_runtime_unload(module);
        }
    }
};

struct Instance::Impl {
    std::shared_ptr<const Module::Impl> module;
    wasm_module_inst_t instance = nullptr;
    wasm_exec_env_t env = nullptr;
    wasm_function_inst_t initFn = nullptr;
    wasm_function_inst_t callbackFn = nullptr;
    std::atomic<bool> interrupted{false};
    detail::WasiState wasi;
    std::vector<detail::Binding> bindings;

    explicit Impl(const WasiHooks& hooks) : wasi(hooks) {}

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ~Impl() {
        if (env != nullptr) {
            wasm_runtime_destroy_exec_env(env);
        }
        if (instance != nullptr) {
            wasm_runtime_deinstantiate(instance);
        }
    }
};

namespace {
struct CallBackend {
    wasm_module_inst_t instance;
};

GuestMemory memoryOf(void* backend) {
    const wasm_memory_inst_t memory = wasm_runtime_get_default_memory(static_cast<CallBackend*>(backend)->instance);
    if (memory == nullptr) {
        return {};
    }
    return {static_cast<std::uint8_t*>(wasm_memory_get_base_address(memory)),
            static_cast<std::size_t>(wasm_memory_get_cur_page_count(memory)) * wasm_memory_get_bytes_per_page(memory)};
}

template <std::size_t I>
void rawTrampoline(wasm_exec_env_t env, std::uint64_t* args) {
    const wasm_module_inst_t instance = wasm_runtime_get_module_inst(env);
    auto* self = static_cast<Instance::Impl*>(wasm_runtime_get_custom_data(instance));
    const detail::Binding& binding = self->bindings[I];
    CallBackend backend{instance};
    Call call(static_cast<std::byte*>(static_cast<void*>(args)), kSlotBytes, &backend, &memoryOf);
    binding.fn(binding.user, call);
    const std::string_view reason = call.trapReason();
    if (!reason.empty()) {
        char text[kErrorBytes];
        const std::size_t length = std::min(reason.size(), sizeof text - 1);
        std::memcpy(text, reason.data(), length);
        text[length] = '\0';
        wasm_runtime_set_exception(instance, text);
    }
}

template <std::size_t... Is>
constexpr std::array<void (*)(wasm_exec_env_t, std::uint64_t*), sizeof...(Is)>
trampolineTable(std::index_sequence<Is...>) {
    return {&rawTrampoline<Is>...};
}

constexpr auto kTrampolines = trampolineTable(std::make_index_sequence<kMaxFunctions>{});

[[nodiscard]] std::string wamrSignature(std::string_view signature) {
    std::string text = "(";
    for (const char c : signature) {
        if (c == '>') {
            text += ')';
            continue;
        }
        text += c == 'l' ? 'I' : c == 'd' ? 'F' : c;
    }
    return text;
}

bool Runtime::acquire() {
    const std::scoped_lock lock(gRuntimeMutex);
    if (gRuntimeUsers == 0) {
        RuntimeInitArgs init;
        std::memset(&init, 0, sizeof init);
        init.mem_alloc_type = Alloc_With_System_Allocator;
#ifdef BASALT_WAMR_FAST_JIT
        init.running_mode = Mode_Fast_JIT;
        init.fast_jit_code_cache_size = 16U * 1024 * 1024;
#endif
        if (!wasm_runtime_full_init(&init)) {
            return false;
        }
        gInitThread = std::this_thread::get_id();
        wasm_runtime_set_log_level(WASM_LOG_LEVEL_FATAL);
        gSignatures.clear();
        gBasaltCount = 0;
        gWasiCount = 0;
        gProxyCount = 0;
        const std::span<const AbiFunction> functions = abiFunctions();
        if (functions.size() > kMaxFunctions) {
            wasm_runtime_destroy();
            return false;
        }
        for (std::size_t index = 0; index < functions.size(); ++index) {
            gSignatures.push_back(wamrSignature(functions[index].signature));
            const NativeSymbol symbol{functions[index].name.data(), reinterpret_cast<void*>(kTrampolines[index]),
                                      gSignatures.back().c_str(), nullptr};
            if (functions[index].module == kBasaltModule) {
                gBasaltSymbols[gBasaltCount++] = symbol;
            } else if (functions[index].module == kProxyModule) {
                gProxySymbols[gProxyCount++] = symbol;
            } else {
                gWasiSymbols[gWasiCount++] = symbol;
            }
        }
        if (!wasm_runtime_register_natives_raw(kBasaltModule.data(), gBasaltSymbols.data(),
                                               static_cast<std::uint32_t>(gBasaltCount)) ||
            !wasm_runtime_register_natives_raw(kProxyModule.data(), gProxySymbols.data(),
                                               static_cast<std::uint32_t>(gProxyCount)) ||
            !wasm_runtime_register_natives_raw(kWasiModule.data(), gWasiSymbols.data(),
                                               static_cast<std::uint32_t>(gWasiCount))) {
            wasm_runtime_destroy();
            return false;
        }
    }
    ++gRuntimeUsers;
    return true;
}

void Runtime::release() {
    const std::scoped_lock lock(gRuntimeMutex);
    if (--gRuntimeUsers == 0) {
        wasm_runtime_destroy();
        gBasaltCount = 0;
        gWasiCount = 0;
    }
}

[[nodiscard]] char kindChar(wasm_import_export_kind_t kind) {
    switch (kind) {
    case WASM_IMPORT_EXPORT_KIND_FUNC:
        return 'f';
    case WASM_IMPORT_EXPORT_KIND_MEMORY:
        return 'm';
    case WASM_IMPORT_EXPORT_KIND_TABLE:
        return 't';
    case WASM_IMPORT_EXPORT_KIND_GLOBAL:
        return 'g';
    default:
        return '?';
    }
}

[[nodiscard]] char valueChar(wasm_valkind_t kind) {
    switch (kind) {
    case WASM_I32:
        return 'i';
    case WASM_I64:
        return 'l';
    case WASM_F32:
        return 'f';
    case WASM_F64:
        return 'd';
    default:
        return 'x';
    }
}

[[nodiscard]] std::string signatureOf(wasm_func_type_t type) {
    std::string text;
    for (std::uint32_t index = 0; index < wasm_func_type_get_param_count(type); ++index) {
        text += valueChar(wasm_func_type_get_param_valkind(type, index));
    }
    text += '>';
    for (std::uint32_t index = 0; index < wasm_func_type_get_result_count(type); ++index) {
        text += valueChar(wasm_func_type_get_result_valkind(type, index));
    }
    return text;
}

[[nodiscard]] detail::ModuleInfo describe(wasm_module_t module) {
    detail::ModuleInfo info;
    const std::int32_t importCount = wasm_runtime_get_import_count(module);
    for (std::int32_t index = 0; index < importCount; ++index) {
        wasm_import_t entry;
        wasm_runtime_get_import_type(module, index, &entry);
        const char kind = kindChar(entry.kind);
        info.imports.push_back(
            {entry.module_name, entry.name, kind, kind == 'f' ? signatureOf(entry.u.func_type) : ""});
    }
    const std::int32_t exportCount = wasm_runtime_get_export_count(module);
    for (std::int32_t index = 0; index < exportCount; ++index) {
        wasm_export_t entry;
        wasm_runtime_get_export_type(module, index, &entry);
        const char kind = kindChar(entry.kind);
        info.exports.push_back({entry.name, kind, kind == 'f' ? signatureOf(entry.u.func_type) : ""});
    }
    return info;
}

[[nodiscard]] Failure failureOf(Instance::Impl& self) {
    const char* message = wasm_runtime_get_exception(self.instance);
    std::string text = message != nullptr ? message : "unknown trap";
    wasm_runtime_clear_exception(self.instance);
    if (self.interrupted.exchange(false, std::memory_order_acq_rel)) {
        return Failure{ErrorCode::Interrupted, "the call was interrupted"};
    }
    return Failure{ErrorCode::Trap, std::move(text)};
}

struct ThreadEnv {
    bool owned = false;

    ThreadEnv() { owned = std::this_thread::get_id() != gInitThread && wasm_runtime_init_thread_env(); }

    ~ThreadEnv() {
        if (owned) {
            wasm_runtime_destroy_thread_env();
        }
    }

    ThreadEnv(const ThreadEnv&) = delete;
    ThreadEnv& operator=(const ThreadEnv&) = delete;
};

void ensureThreadEnv() { thread_local const ThreadEnv env; }

[[nodiscard]] Result<void> run(Instance::Impl& self, wasm_function_inst_t function, std::uint32_t* argv,
                               std::uint32_t argc) {
    ensureThreadEnv();
    self.interrupted.store(false, std::memory_order_relaxed);
    wasm_runtime_clear_exception(self.instance);
    if (wasm_runtime_call_wasm(self.env, function, argc, argv)) {
        return {};
    }
    return std::unexpected(failureOf(self));
}
} // namespace

Result<Engine> Engine::create(const EngineLimits& limits) {
    auto impl = std::make_shared<Impl>();
    impl->limits = limits;
    if (!Runtime::acquire()) {
        return fail(ErrorCode::Backend, "wamr could not initialise its runtime");
    }
    impl->holdsRuntime = true;
    return Engine(std::move(impl));
}

const EngineLimits& Engine::limits() const noexcept { return impl_->limits; }

std::string_view Engine::backend() const noexcept { return BASALT_WASM_BACKEND_ID; }

bool Engine::supportsSerialization() const noexcept { return false; }

Result<Module> Module::compile(const Engine& engine, std::span<const std::uint8_t> wasm) {
    auto impl = std::make_unique<Impl>();
    impl->engine = engine.impl_;
    impl->bytes.assign(wasm.begin(), wasm.end());
    char error[kErrorBytes];
    impl->module =
        wasm_runtime_load(impl->bytes.data(), static_cast<std::uint32_t>(impl->bytes.size()), error, sizeof error);
    if (impl->module == nullptr) {
        return fail(ErrorCode::Compile, error);
    }
    impl->info = describe(impl->module);
    if (auto ok = detail::validateModule(impl->info, impl->engine->limits.profile); !ok) {
        return std::unexpected(ok.error());
    }
    return Module(std::shared_ptr<const Impl>(std::move(impl)));
}

Result<Module> Module::deserialize(const Engine&, std::span<const std::uint8_t>) {
    return fail(ErrorCode::Unsupported, "the wamr backend has no compiled-module format");
}

Result<std::vector<std::uint8_t>> Module::serialize() const {
    return fail(ErrorCode::Unsupported, "the wamr backend has no compiled-module format");
}

Instance::Instance(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Instance::Instance(Instance&&) noexcept = default;
Instance& Instance::operator=(Instance&&) noexcept = default;
Instance::~Instance() = default;

Result<Instance> Instance::create(const Module& module, const InstanceOptions& options) {
    ensureThreadEnv();
    const Module::Impl& compiled = *module.impl_;
    auto impl = std::make_unique<Impl>(options.wasi);
    Impl& self = *impl;
    self.module = module.impl_;

    auto bindings = detail::buildBindings(compiled.info, options.imports, self.wasi);
    if (!bindings) {
        return std::unexpected(bindings.error());
    }
    self.bindings = std::move(*bindings);

    const EngineLimits& limits = compiled.engine->limits;
    InstantiationArgs2* args = nullptr;
    if (!wasm_runtime_instantiation_args_create(&args)) {
        return fail(ErrorCode::Instantiate, "wamr could not allocate instantiation arguments");
    }
    wasm_runtime_instantiation_args_set_default_stack_size(args, static_cast<std::uint32_t>(limits.maxStackBytes));
    wasm_runtime_instantiation_args_set_host_managed_heap_size(args, 0);
    wasm_runtime_instantiation_args_set_max_memory_pages(
        args, static_cast<std::uint32_t>(std::min<std::uint64_t>(limits.maxMemoryBytes / kWasmPageBytes, 65536)));
    char error[kErrorBytes];
    self.instance = wasm_runtime_instantiate_ex2(compiled.module, args, error, sizeof error);
    wasm_runtime_instantiation_args_destroy(args);
    if (self.instance == nullptr) {
        return fail(ErrorCode::Instantiate, error);
    }
    wasm_runtime_set_custom_data(self.instance, &self);
#ifdef BASALT_WAMR_FAST_JIT
    (void)wasm_runtime_set_running_mode(self.instance, Mode_Fast_JIT);
#endif
    self.env = wasm_runtime_create_exec_env(self.instance, static_cast<std::uint32_t>(limits.maxStackBytes));
    if (self.env == nullptr) {
        return fail(ErrorCode::Instantiate, "wamr could not create an execution environment");
    }
    self.initFn = wasm_runtime_lookup_function(self.instance, "basalt_init");
    self.callbackFn = wasm_runtime_lookup_function(self.instance, "basalt_callback");
    const wasm_function_inst_t version = wasm_runtime_lookup_function(self.instance, "basalt_abi_version");
    if (self.initFn == nullptr || self.callbackFn == nullptr || version == nullptr ||
        wasm_runtime_get_default_memory(self.instance) == nullptr) {
        return fail(ErrorCode::InvalidModule, "a required export disappeared at instantiation");
    }

    if (const wasm_function_inst_t initialize = wasm_runtime_lookup_function(self.instance, "_initialize");
        initialize != nullptr) {
        if (auto ran = run(self, initialize, nullptr, 0); !ran) {
            return std::unexpected(ran.error());
        }
    }

    std::uint32_t result[2] = {0, 0};
    if (auto ran = run(self, version, result, 0); !ran) {
        return std::unexpected(ran.error());
    }
    if (static_cast<std::int32_t>(result[0]) != kAbiVersion) {
        return fail(ErrorCode::AbiVersion, "the plugin was built for ABI " +
                                               std::to_string(static_cast<std::int32_t>(result[0])) +
                                               ", this host speaks " + std::to_string(kAbiVersion));
    }
    return Instance(std::move(impl));
}

Result<void> Instance::init() { return run(*impl_, impl_->initFn, nullptr, 0); }

Result<void> Instance::callback(std::int32_t token, std::int32_t ctx) {
    std::uint32_t args[2] = {static_cast<std::uint32_t>(token), static_cast<std::uint32_t>(ctx)};
    return run(*impl_, impl_->callbackFn, args, 2);
}

void Instance::interrupt() noexcept {
    impl_->interrupted.store(true, std::memory_order_release);
    wasm_runtime_terminate(impl_->instance);
}

GuestMemory Instance::memory() const noexcept {
    CallBackend backend{impl_->instance};
    return memoryOf(&backend);
}

} // namespace basalt::wire::wasm
