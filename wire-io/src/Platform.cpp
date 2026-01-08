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
#include <poll.h>
#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#else
#include <cstdio>
#include <poll.h>
#include <sys/resource.h>
#include <termios.h>
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

std::optional<std::chrono::microseconds> processCpuTime() {
#if defined(_WIN32)
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user) == 0) {
        return std::nullopt;
    }
    const auto toMicros = [](const FILETIME& time) {
        const std::uint64_t hundredNanos = (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
        return hundredNanos / 10;
    };
    return std::chrono::microseconds{toMicros(kernel) + toMicros(user)};
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return std::nullopt;
    }
    const auto micros = [](const timeval& time) {
        return static_cast<std::int64_t>(time.tv_sec) * 1'000'000 + time.tv_usec;
    };
    return std::chrono::microseconds{micros(usage.ru_utime) + micros(usage.ru_stime)};
#endif
}

std::optional<unsigned int> cpuCoreCount() {
    const unsigned int cores = std::thread::hardware_concurrency();
    return cores == 0 ? std::nullopt : std::optional{cores};
}

namespace terminal {
namespace {

#if !defined(_WIN32)
// The settings as they were before this process touched them, kept so they can be put back.
termios gSaved{};
bool gRaw = false;
#endif

} // namespace

bool enableRawMode() {
#if defined(_WIN32)
    // The Windows console has its own mode flags rather than termios; the line editor falls back to
    // plain reading there until that is written.
    return false;
#else
    if (::isatty(STDIN_FILENO) != 1) {
        return false;
    }
    if (gRaw) {
        return true;
    }
    if (::tcgetattr(STDIN_FILENO, &gSaved) != 0) {
        return false;
    }

    termios raw = gSaved;
    // Only line buffering and echo go: signals and output processing stay exactly as they were, so
    // the terminal still behaves like a terminal in every other respect.
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return false;
    }
    gRaw = true;
    return true;
#endif
}

void restoreMode() {
#if !defined(_WIN32)
    if (gRaw) {
        ::tcsetattr(STDIN_FILENO, TCSANOW, &gSaved);
        gRaw = false;
    }
#endif
}

int readByte() { return std::getchar(); }

bool waitForInput(const std::chrono::milliseconds timeout) {
#if defined(_WIN32)
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (input == nullptr || input == INVALID_HANDLE_VALUE) {
        return true;
    }
    return WaitForSingleObject(input, static_cast<DWORD>(timeout.count())) == WAIT_OBJECT_0;
#else
    pollfd watched{};
    watched.fd = STDIN_FILENO;
    watched.events = POLLIN;
    const int ready = ::poll(&watched, 1, static_cast<int>(timeout.count()));
    // An error is reported as ready so the read runs and returns whatever it returns: pretending
    // there is nothing would spin here forever.
    return ready != 0;
#endif
}

} // namespace terminal

} // namespace basalt::wire::io
