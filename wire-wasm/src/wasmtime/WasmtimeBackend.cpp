#include "../Internal.hpp"

#include <basalt/wire/wasm/Abi.hpp>

#include <atomic>
#include <cstring>
#include <wasmtime.h>

#ifndef BASALT_WASM_BACKEND_ID
#error "the build must define BASALT_WASM_BACKEND_ID"
#endif

namespace basalt::wire::wasm {

struct Engine::Impl {
    EngineLimits limits;
    wasm_engine_t* engine = nullptr;

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ~Impl() {
        if (engine != nullptr) {
            wasm_engine_delete(engine);
        }
    }
};

struct Module::Impl {
    std::shared_ptr<Engine::Impl> engine;
    wasmtime_module_t* module = nullptr;
    detail::ModuleInfo info;

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ~Impl() {
        if (module != nullptr) {
            wasmtime_module_delete(module);
        }
    }
};

namespace {
struct Bound {
    Instance::Impl* self = nullptr;
    HostFn fn = nullptr;
    void* user = nullptr;
};

struct CallBackend {
    wasmtime_caller_t* caller;
    Instance::Impl* self;
};
} // namespace

struct Instance::Impl {
    std::shared_ptr<const Module::Impl> module;
    wasmtime_store_t* store = nullptr;
    wasmtime_context_t* context = nullptr;
    wasmtime_memory_t memory{};
    wasmtime_func_t initFn{};
    wasmtime_func_t callbackFn{};
    std::atomic<bool> interrupted{false};
    detail::WasiState wasi;
    std::vector<Bound> bound;

    explicit Impl(const WasiHooks& hooks) : wasi(hooks) {}

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ~Impl() {
        if (store != nullptr) {
            wasmtime_store_delete(store);
        }
    }
};

namespace {
[[nodiscard]] std::string messageOf(wasmtime_error_t* error) {
    wasm_byte_vec_t message;
    wasmtime_error_message(error, &message);
    std::string text(message.data, message.size);
    wasm_byte_vec_delete(&message);
    wasmtime_error_delete(error);
    return text;
}

[[nodiscard]] std::string messageOf(wasm_trap_t* trap) {
    wasm_byte_vec_t message;
    wasm_trap_message(trap, &message);
    std::string text(message.data, message.size);
    wasm_byte_vec_delete(&message);
    wasm_trap_delete(trap);
    return text;
}

[[nodiscard]] char kindChar(wasm_externkind_t kind) {
    switch (kind) {
    case WASM_EXTERN_FUNC:
        return 'f';
    case WASM_EXTERN_MEMORY:
        return 'm';
    case WASM_EXTERN_TABLE:
        return 't';
    case WASM_EXTERN_GLOBAL:
        return 'g';
    default:
        return '?';
    }
}

[[nodiscard]] char valueChar(const wasm_valtype_t* type) {
    switch (wasm_valtype_kind(type)) {
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

[[nodiscard]] std::string signatureOf(const wasm_externtype_t* type) {
    const wasm_functype_t* function = wasm_externtype_as_functype_const(type);
    std::string text;
    const wasm_valtype_vec_t* params = wasm_functype_params(function);
    for (std::size_t index = 0; index < params->size; ++index) {
        text += valueChar(params->data[index]);
    }
    text += '>';
    const wasm_valtype_vec_t* results = wasm_functype_results(function);
    for (std::size_t index = 0; index < results->size; ++index) {
        text += valueChar(results->data[index]);
    }
    return text;
}

[[nodiscard]] detail::ModuleInfo describe(const wasmtime_module_t* module) {
    detail::ModuleInfo info;
    wasm_importtype_vec_t imports;
    wasmtime_module_imports(module, &imports);
    for (std::size_t index = 0; index < imports.size; ++index) {
        const wasm_importtype_t* entry = imports.data[index];
        const wasm_name_t* moduleName = wasm_importtype_module(entry);
        const wasm_name_t* name = wasm_importtype_name(entry);
        const wasm_externtype_t* type = wasm_importtype_type(entry);
        const char kind = kindChar(wasm_externtype_kind(type));
        info.imports.push_back({std::string(moduleName->data, moduleName->size), std::string(name->data, name->size),
                                kind, kind == 'f' ? signatureOf(type) : std::string{}});
    }
    wasm_importtype_vec_delete(&imports);
    wasm_exporttype_vec_t exports;
    wasmtime_module_exports(module, &exports);
    for (std::size_t index = 0; index < exports.size; ++index) {
        const wasm_exporttype_t* entry = exports.data[index];
        const wasm_name_t* name = wasm_exporttype_name(entry);
        const wasm_externtype_t* type = wasm_exporttype_type(entry);
        const char kind = kindChar(wasm_externtype_kind(type));
        info.exports.push_back(
            {std::string(name->data, name->size), kind, kind == 'f' ? signatureOf(type) : std::string{}});
    }
    wasm_exporttype_vec_delete(&exports);
    return info;
}

[[nodiscard]] Result<Module::Impl*> finish(std::unique_ptr<Module::Impl>& impl) {
    impl->info = describe(impl->module);
    if (auto ok = detail::validateModule(impl->info, impl->engine->limits.profile); !ok) {
        return std::unexpected(ok.error());
    }
    return impl.get();
}

[[nodiscard]] wasm_functype_t* functionType(std::string_view signature) {
    wasm_valtype_vec_t params;
    wasm_valtype_vec_t results;
    wasm_valtype_vec_new_empty(&params);
    wasm_valtype_vec_new_empty(&results);
    wasm_valtype_vec_t* target = &params;
    auto kindOf = [](char c) {
        switch (c) {
        case 'i':
            return WASM_I32;
        case 'l':
            return WASM_I64;
        case 'f':
            return WASM_F32;
        default:
            return WASM_F64;
        }
    };
    std::size_t paramCount = signature.find('>');
    wasm_valtype_vec_new_uninitialized(&params, paramCount);
    wasm_valtype_vec_new_uninitialized(&results, signature.size() - paramCount - 1);
    for (std::size_t index = 0; index < signature.size(); ++index) {
        if (index == paramCount) {
            target = &results;
            continue;
        }
        const std::size_t slot = index < paramCount ? index : index - paramCount - 1;
        target->data[slot] = wasm_valtype_new(kindOf(signature[index]));
    }
    return wasm_functype_new(&params, &results);
}

GuestMemory memoryOf(void* backend) {
    auto* frame = static_cast<CallBackend*>(backend);
    wasmtime_context_t* context = wasmtime_caller_context(frame->caller);
    return {wasmtime_memory_data(context, &frame->self->memory),
            wasmtime_memory_data_size(context, &frame->self->memory)};
}

wasm_trap_t* trampoline(void* env, wasmtime_caller_t* caller, wasmtime_val_raw_t* slots, std::size_t) {
    auto* bound = static_cast<Bound*>(env);
    CallBackend backend{caller, bound->self};
    Call call(static_cast<std::byte*>(static_cast<void*>(slots)), sizeof(wasmtime_val_raw_t), &backend, &memoryOf);
    bound->fn(bound->user, call);
    const std::string_view reason = call.trapReason();
    return reason.empty() ? nullptr : wasmtime_trap_new(reason.data(), reason.size());
}

wasmtime_error_t* onDeadline(const wasmtime_context_t*, void* data, std::uint64_t* delta,
                             wasmtime_update_deadline_kind_t*) {
    auto* self = static_cast<Instance::Impl*>(data);
    if (self->interrupted.load(std::memory_order_acquire)) {
        return wasmtime_error_new("interrupted");
    }
    *delta = 1;
    return nullptr;
}

[[nodiscard]] Failure failureOf(Instance::Impl& self, wasmtime_error_t* error, wasm_trap_t* trap) {
    std::string text = trap != nullptr ? messageOf(trap) : messageOf(error);
    if (trap != nullptr && error != nullptr) {
        wasmtime_error_delete(error);
    }
    if (self.interrupted.exchange(false, std::memory_order_acq_rel)) {
        return Failure{ErrorCode::Interrupted, "the call was interrupted"};
    }
    return Failure{ErrorCode::Trap, std::move(text)};
}

[[nodiscard]] Result<void> run(Instance::Impl& self, const wasmtime_func_t& function, wasmtime_val_raw_t* slots,
                               std::size_t count) {
    self.interrupted.store(false, std::memory_order_relaxed);
    wasm_trap_t* trap = nullptr;
    wasmtime_error_t* error = wasmtime_func_call_unchecked(self.context, &function, slots, count, &trap);
    if (error == nullptr && trap == nullptr) {
        return {};
    }
    return std::unexpected(failureOf(self, error, trap));
}

[[nodiscard]] bool exportOf(Instance::Impl& self, const wasmtime_instance_t& instance, std::string_view name,
                            wasmtime_extern_kind_t kind, wasmtime_extern_t& out) {
    return wasmtime_instance_export_get(self.context, &instance, name.data(), name.size(), &out) && out.kind == kind;
}
} // namespace

Result<Engine> Engine::create(const EngineLimits& limits) {
    wasm_config_t* config = wasm_config_new();
    wasmtime_config_strategy_set(config, WASMTIME_STRATEGY_CRANELIFT);
    wasmtime_config_cranelift_opt_level_set(config, WASMTIME_OPT_LEVEL_SPEED);
    wasmtime_config_epoch_interruption_set(config, true);
    wasmtime_config_max_wasm_stack_set(config, limits.maxStackBytes);
    wasmtime_config_wasm_function_references_set(config, true);
    wasmtime_config_wasm_gc_set(config, true);
    wasmtime_config_wasm_exceptions_set(config, true);
    wasmtime_config_wasm_tail_call_set(config, true);
    auto impl = std::make_shared<Impl>();
    impl->limits = limits;
    impl->engine = wasm_engine_new_with_config(config);
    if (impl->engine == nullptr) {
        return fail(ErrorCode::Backend, "wasmtime could not create an engine");
    }
    return Engine(std::move(impl));
}

const EngineLimits& Engine::limits() const noexcept { return impl_->limits; }

std::string_view Engine::backend() const noexcept { return BASALT_WASM_BACKEND_ID; }

bool Engine::supportsSerialization() const noexcept { return true; }

Result<Module> Module::compile(const Engine& engine, std::span<const std::uint8_t> wasm) {
    auto impl = std::make_unique<Impl>();
    impl->engine = engine.impl_;
    if (wasmtime_error_t* error = wasmtime_module_new(engine.impl_->engine, wasm.data(), wasm.size(), &impl->module)) {
        return fail(ErrorCode::Compile, messageOf(error));
    }
    if (auto ok = finish(impl); !ok) {
        return std::unexpected(ok.error());
    }
    return Module(std::shared_ptr<const Impl>(std::move(impl)));
}

Result<Module> Module::deserialize(const Engine& engine, std::span<const std::uint8_t> compiled) {
    auto impl = std::make_unique<Impl>();
    impl->engine = engine.impl_;
    if (wasmtime_error_t* error =
            wasmtime_module_deserialize(engine.impl_->engine, compiled.data(), compiled.size(), &impl->module)) {
        return fail(ErrorCode::Compile, messageOf(error));
    }
    if (auto ok = finish(impl); !ok) {
        return std::unexpected(ok.error());
    }
    return Module(std::shared_ptr<const Impl>(std::move(impl)));
}

Result<std::vector<std::uint8_t>> Module::serialize() const {
    wasm_byte_vec_t bytes;
    if (wasmtime_error_t* error = wasmtime_module_serialize(impl_->module, &bytes)) {
        return fail(ErrorCode::Backend, messageOf(error));
    }
    std::vector<std::uint8_t> copy(bytes.size);
    std::memcpy(copy.data(), bytes.data, bytes.size);
    wasm_byte_vec_delete(&bytes);
    return copy;
}

Instance::Instance(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Instance::Instance(Instance&&) noexcept = default;
Instance& Instance::operator=(Instance&&) noexcept = default;
Instance::~Instance() = default;

Result<Instance> Instance::create(const Module& module, const InstanceOptions& options) {
    const Module::Impl& compiled = *module.impl_;
    auto impl = std::make_unique<Impl>(options.wasi);
    Impl& self = *impl;
    self.module = module.impl_;

    auto bindings = detail::buildBindings(compiled.info, options.imports, self.wasi);
    if (!bindings) {
        return std::unexpected(bindings.error());
    }

    const EngineLimits& limits = compiled.engine->limits;
    self.store = wasmtime_store_new(compiled.engine->engine, nullptr, nullptr);
    self.context = wasmtime_store_context(self.store);
    wasmtime_store_limiter(self.store, static_cast<std::int64_t>(limits.maxMemoryBytes), 1'000'000, -1, -1, -1);
    wasmtime_context_set_epoch_deadline(self.context, 1);
    wasmtime_store_epoch_deadline_callback(self.store, onDeadline, &self, nullptr);

    self.bound.resize(compiled.info.imports.size());
    std::vector<wasmtime_extern_t> externs(compiled.info.imports.size());
    for (std::size_t index = 0; index < compiled.info.imports.size(); ++index) {
        const detail::ImportInfo& entry = compiled.info.imports[index];
        const std::size_t abiIndex = *findAbiFunction(entry.module, entry.name);
        self.bound[index] = Bound{&self, (*bindings)[abiIndex].fn, (*bindings)[abiIndex].user};
        wasm_functype_t* type = functionType(abiFunctions()[abiIndex].signature);
        wasmtime_func_t function;
        wasmtime_func_new_unchecked(self.context, type, trampoline, &self.bound[index], nullptr, &function);
        wasm_functype_delete(type);
        externs[index].kind = WASMTIME_EXTERN_FUNC;
        externs[index].of.func = function;
    }

    wasmtime_instance_t instance;
    wasm_trap_t* trap = nullptr;
    if (wasmtime_error_t* error =
            wasmtime_instance_new(self.context, compiled.module, externs.data(), externs.size(), &instance, &trap)) {
        return fail(ErrorCode::Instantiate, messageOf(error));
    }
    if (trap != nullptr) {
        return fail(ErrorCode::Instantiate, messageOf(trap));
    }

    wasmtime_extern_t memory;
    wasmtime_extern_t init;
    wasmtime_extern_t callback;
    wasmtime_extern_t version;
    if (!exportOf(self, instance, "memory", WASMTIME_EXTERN_MEMORY, memory) ||
        !exportOf(self, instance, "basalt_init", WASMTIME_EXTERN_FUNC, init) ||
        !exportOf(self, instance, "basalt_callback", WASMTIME_EXTERN_FUNC, callback) ||
        !exportOf(self, instance, "basalt_abi_version", WASMTIME_EXTERN_FUNC, version)) {
        return fail(ErrorCode::InvalidModule, "a required export disappeared at instantiation");
    }
    self.memory = memory.of.memory;
    self.initFn = init.of.func;
    self.callbackFn = callback.of.func;

    wasmtime_extern_t initialize;
    if (wasmtime_instance_export_get(self.context, &instance, "_initialize", 11, &initialize) &&
        initialize.kind == WASMTIME_EXTERN_FUNC) {
        if (auto ran = run(self, initialize.of.func, nullptr, 0); !ran) {
            return std::unexpected(ran.error());
        }
    }

    wasmtime_val_raw_t slot[1];
    slot[0].i32 = 0;
    if (auto ran = run(self, version.of.func, slot, 1); !ran) {
        return std::unexpected(ran.error());
    }
    if (slot[0].i32 != kAbiVersion) {
        return fail(ErrorCode::AbiVersion, "the plugin was built for ABI " + std::to_string(slot[0].i32) +
                                               ", this host speaks " + std::to_string(kAbiVersion));
    }
    return Instance(std::move(impl));
}

Result<void> Instance::init() { return run(*impl_, impl_->initFn, nullptr, 0); }

Result<void> Instance::callback(std::int32_t token, std::int32_t ctx) {
    wasmtime_val_raw_t args[2];
    args[0].i32 = token;
    args[1].i32 = ctx;
    return run(*impl_, impl_->callbackFn, args, 2);
}

void Instance::interrupt() noexcept {
    impl_->interrupted.store(true, std::memory_order_release);
    wasmtime_engine_increment_epoch(impl_->module->engine->engine);
}

GuestMemory Instance::memory() const noexcept {
    return {wasmtime_memory_data(impl_->context, &impl_->memory),
            wasmtime_memory_data_size(impl_->context, &impl_->memory)};
}

} // namespace basalt::wire::wasm
