#include <basalt/wire/shim/ShimHost.hpp>

#include <array>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace basalt::wire::shim {
namespace {
using CreateVm = jint (*)(JavaVM**, void**, void*);

[[nodiscard]] std::string joined(const std::vector<std::string>& paths) {
    std::string out;
    for (const std::string& path : paths) {
        if (!out.empty()) {
#ifdef _WIN32
            out += ';';
#else
            out += ':';
#endif
        }
        out += path;
    }
    return out;
}
} // namespace

ShimHost::~ShimHost() = default;

std::expected<std::unique_ptr<ShimHost>, std::string> ShimHost::create(const ShimOptions& options) {
#ifdef _WIN32
    HMODULE module = LoadLibraryA(options.libjvm.c_str());
    if (module == nullptr) {
        return std::unexpected(std::string{"cannot load "} + options.libjvm + ": error " +
                               std::to_string(GetLastError()));
    }
    void* library = module;
    void* symbol = reinterpret_cast<void*>(GetProcAddress(module, "JNI_CreateJavaVM"));
#else
    void* library = dlopen(options.libjvm.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (library == nullptr) {
        return std::unexpected(std::string{"cannot load "} + options.libjvm + ": " + dlerror());
    }
    void* symbol = dlsym(library, "JNI_CreateJavaVM");
#endif
    if (symbol == nullptr) {
        return std::unexpected(options.libjvm + " has no JNI_CreateJavaVM");
    }
    CreateVm create = nullptr;
    static_assert(sizeof(create) == sizeof(symbol));
    __builtin_memcpy(&create, &symbol, sizeof(create));

    const std::string classpath = "-Djava.class.path=" + joined(options.classpath);
    const std::string boot = "-Xbootclasspath:" + options.bootClasspath;
    const std::string bootstrap = "-Dshim.bootstrap=" + options.libjvm;
    std::array<JavaVMOption, 3> vmOptions{};
    vmOptions[0].optionString = const_cast<char*>(classpath.c_str());
    vmOptions[1].optionString = const_cast<char*>(boot.c_str());
    vmOptions[2].optionString = const_cast<char*>(bootstrap.c_str());
    JavaVMInitArgs args{};
    args.version = JNI_VERSION_1_8;
    args.nOptions = static_cast<jint>(vmOptions.size());
    args.options = vmOptions.data();

    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    if (create(&vm, reinterpret_cast<void**>(&env), &args) != JNI_OK) {
        return std::unexpected(std::string{"JNI_CreateJavaVM failed"});
    }
    return std::make_unique<ShimHost>(Key{}, library, vm);
}

namespace {
// A connection thread attaches on first use and must leave the VM before it exits.
std::atomic<bool> gDestroyed{false};
std::atomic<bool> gStopping{false};
std::atomic<bool> gLeftRunning{false};

struct Attachment {
    JavaVM* vm = nullptr;

    Attachment() = default;
    Attachment(const Attachment&) = delete;
    Attachment& operator=(const Attachment&) = delete;

    ~Attachment() {
        if (vm != nullptr && !gStopping.load()) {
            vm->DetachCurrentThread();
        }
    }
};

thread_local Attachment tAttachment;
} // namespace

bool ShimHost::leftRunning() { return gLeftRunning.load(); }

void ShimHost::markLeftRunning() { gLeftRunning.store(true); }

void ShimHost::beginShutdown() { gStopping.store(true); }

void ShimHost::destroy() {
    if (gDestroyed.exchange(true)) {
        return;
    }
    vm_->DestroyJavaVM();
}

JNIEnv* ShimHost::env() const {
    JNIEnv* env = nullptr;
    if (gDestroyed.load()) {
        return nullptr;
    }
    const jint status = vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
    if (status == JNI_OK) {
        return env;
    }
    // Only detached native threads become ours to detach at thread exit.
    if (status != JNI_EDETACHED) {
        return nullptr;
    }
    if (vm_->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
        return nullptr;
    }

    tAttachment.vm = vm_;
    return env;
}
} // namespace basalt::wire::shim
