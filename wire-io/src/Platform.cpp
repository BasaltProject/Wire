#include <basalt/wire/io/Platform.hpp>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <thread>

#if defined(_WIN32)
#include <io.h>
#include <psapi.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <unistd.h>
#else
#include <cstdio>
#include <unistd.h>
#endif

namespace basalt::wire::io {
namespace {

std::atomic_flag gShutdownRequested = ATOMIC_FLAG_INIT;

extern "C" void onTerminationSignal(int) { gShutdownRequested.test_and_set(std::memory_order_relaxed); }

} // namespace

namespace shutdownSignal {

void install() {
    gShutdownRequested.clear(std::memory_order_relaxed);
    std::signal(SIGINT, onTerminationSignal);
    std::signal(SIGTERM, onTerminationSignal);
}

bool isRequested() noexcept { return gShutdownRequested.test(std::memory_order_relaxed); }

void reset() noexcept { gShutdownRequested.clear(std::memory_order_relaxed); }

} // namespace shutdownSignal

void flushConsole() { std::fflush(stdout); }

void sleepFor(std::chrono::milliseconds duration) { std::this_thread::sleep_for(duration); }

std::uint64_t residentSetBytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) != 0) {
        return counters.WorkingSetSize;
    }
    return 0;
#elif defined(__APPLE__)
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) ==
        KERN_SUCCESS) {
        return info.resident_size;
    }
    return 0;
#else
    // /proc/self/statm reports the resident set in pages, as its second field.
    std::FILE* statm = std::fopen("/proc/self/statm", "r");
    if (statm == nullptr) {
        return 0;
    }
    unsigned long total = 0;
    unsigned long resident = 0;
    const int read = std::fscanf(statm, "%lu %lu", &total, &resident);
    std::fclose(statm);
    if (read != 2) {
        return 0;
    }
    return static_cast<std::uint64_t>(resident) * static_cast<std::uint64_t>(::sysconf(_SC_PAGESIZE));
#endif
}

bool isConsoleInteractive() {
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return ::isatty(fileno(stdout)) != 0;
#endif
}

} // namespace basalt::wire::io
