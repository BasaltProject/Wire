#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::common {
class Timings;

/// Names the calling thread for the operating system, so a profile and a debugger both show it.
void nameCurrentThread(std::string_view name);

/// Samples the C++ stacks of the process's busy threads while a `Timings` is measuring, so a slow spot is reported
/// as the function that was running and not only as the section that held it.
///
/// A signal makes the sampled thread park inside its handler while this one reads its frames, so the handler itself
/// does nothing that is unsafe in a signal. Frames are followed through the frame pointer, which is why Linux builds
/// keep it.
class NativeSampler {
public:
    explicit NativeSampler(const Timings& timings);
    ~NativeSampler();
    NativeSampler(const NativeSampler&) = delete;
    NativeSampler& operator=(const NativeSampler&) = delete;

    /// Only Linux (glibc and musl) and macOS can sample; elsewhere `start` does nothing and no thread is reported.
    [[nodiscard]] static bool supported() noexcept;

    void start();

    void stop();

    /// One JSON object per thread, in the shape of the profiler report's own threads. Function names are looked up
    /// here and not while sampling, so the cost of a sample stays the cost of reading a few frames.
    [[nodiscard]] std::vector<std::string> threadsJson() const;

private:
    struct State;
    const Timings* timings_;
    std::unique_ptr<State> state_;
};

/// Turns return addresses of this process into function names; exposed so the lookup can be tested alone.
class StackSymbols {
public:
    StackSymbols();
    ~StackSymbols();
    StackSymbols(const StackSymbols&) = delete;
    StackSymbols& operator=(const StackSymbols&) = delete;

    /// The demangled function without its arguments, or the module and offset when no symbol covers `address`.
    /// `returnAddress` backs the address up into the call that made it.
    [[nodiscard]] std::string name(std::uintptr_t address, bool returnAddress);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// `ns::Class::method(int) const` becomes `ns::Class::method`.
[[nodiscard]] std::string withoutArguments(std::string_view demangled);
} // namespace basalt::wire::common
