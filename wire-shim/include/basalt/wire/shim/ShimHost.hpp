#pragma once

#include <expected>
#include <jni.h>
#include <memory>
#include <string>
#include <vector>

namespace basalt::wire::shim {
struct ShimOptions {
    std::string libjvm;
    std::string bootClasspath;
    std::vector<std::string> classpath;
};

class ShimHost {
    struct Key {};

public:
    ShimHost(Key, void* library, JavaVM* vm) : library_(library), vm_(vm) {}

    ShimHost(const ShimHost&) = delete;
    ShimHost& operator=(const ShimHost&) = delete;
    ShimHost(ShimHost&&) = delete;
    ShimHost& operator=(ShimHost&&) = delete;
    ~ShimHost();

    [[nodiscard]] static std::expected<std::unique_ptr<ShimHost>, std::string> create(const ShimOptions& options);

    [[nodiscard]] JavaVM* vm() const noexcept { return vm_; }

    /// True when Java threads outlived the shutdown, so the VM could not be destroyed and the process must _Exit.
    [[nodiscard]] static bool leftRunning();
    static void markLeftRunning();

    /// Threads that exit from now on stay attached: detaching while Java still runs aborts the VM.
    void beginShutdown();

    /// Destroys the VM from the calling thread; no thread may use JNI afterwards, and env() returns null.
    void destroy();

    /// The environment of the calling thread, attaching it to the VM on first use.
    [[nodiscard]] JNIEnv* env() const;

private:
    [[maybe_unused]] void* library_;
    JavaVM* vm_;
};
} // namespace basalt::wire::shim
