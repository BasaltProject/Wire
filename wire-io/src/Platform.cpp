#include <basalt/wire/io/Platform.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <string>
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

#if defined(_WIN32)
DWORD gSavedInput = 0;
DWORD gSavedOutput = 0;
bool gOutputSaved = false;
bool gRaw = false;

[[nodiscard]] bool yieldsInput(const INPUT_RECORD& record) {
    return record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown != 0 &&
           record.Event.KeyEvent.uChar.UnicodeChar != 0;
}
#else
termios gSaved{};
bool gRaw = false;
#endif

} // namespace

bool enableRawMode() {
#if defined(_WIN32)
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (input == nullptr || input == INVALID_HANDLE_VALUE) {
        return false;
    }
    if (gRaw) {
        return true;
    }
    if (GetConsoleMode(input, &gSavedInput) == 0) {
        return false;
    }
    const DWORD raw = (gSavedInput & ~static_cast<DWORD>(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)) |
                      ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT;
    if (SetConsoleMode(input, raw) == 0) {
        return false;
    }
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output != nullptr && output != INVALID_HANDLE_VALUE && GetConsoleMode(output, &gSavedOutput) != 0) {
        gOutputSaved = true;
        SetConsoleMode(output, gSavedOutput | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    gRaw = true;
    return true;
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
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return false;
    }
    gRaw = true;
    return true;
#endif
}

void restoreMode() {
#if defined(_WIN32)
    if (gRaw) {
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), gSavedInput);
        if (gOutputSaved) {
            SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), gSavedOutput);
            gOutputSaved = false;
        }
        gRaw = false;
    }
#else
    if (gRaw) {
        ::tcsetattr(STDIN_FILENO, TCSANOW, &gSaved);
        gRaw = false;
    }
#endif
}

int readByte() {
#if defined(_WIN32)
    static std::string pending;
    if (!gRaw) {
        return std::getchar();
    }
    while (pending.empty()) {
        wchar_t units[2]{};
        DWORD read = 0;
        if (ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), units, 1, &read, nullptr) == 0 || read == 0) {
            return -1;
        }
        if (units[0] >= 0xD800 && units[0] <= 0xDBFF &&
            (ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), &units[1], 1, &read, nullptr) == 0 || read == 0)) {
            return -1;
        }
        const int length = units[1] != 0 ? 2 : 1;
        char bytes[8]{};
        const int written =
            WideCharToMultiByte(CP_UTF8, 0, units, length, bytes, static_cast<int>(sizeof(bytes)), nullptr, nullptr);
        pending.assign(bytes, static_cast<std::size_t>(std::max(written, 0)));
    }
    const auto byte = static_cast<unsigned char>(pending.front());
    pending.erase(0, 1);
    return byte;
#else
    unsigned char byte = 0;
    return ::read(STDIN_FILENO, &byte, 1) == 1 ? byte : -1;
#endif
}

bool waitForInput(const std::chrono::milliseconds timeout) {
#if defined(_WIN32)
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (input == nullptr || input == INVALID_HANDLE_VALUE) {
        return true;
    }
    DWORD mode = 0;
    if (GetConsoleMode(input, &mode) == 0) {
        return WaitForSingleObject(input, static_cast<DWORD>(timeout.count())) == WAIT_OBJECT_0;
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        const auto left =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (WaitForSingleObject(input, static_cast<DWORD>(std::max<std::int64_t>(left.count(), 0))) != WAIT_OBJECT_0) {
            return false;
        }
        INPUT_RECORD record{};
        DWORD count = 0;
        if (PeekConsoleInputW(input, &record, 1, &count) == 0 || count == 0) {
            return true;
        }
        if (yieldsInput(record)) {
            return true;
        }
        ReadConsoleInputW(input, &record, 1, &count);
    }
#else
    pollfd watched{};
    watched.fd = STDIN_FILENO;
    watched.events = POLLIN;
    const int ready = ::poll(&watched, 1, static_cast<int>(timeout.count()));
    return ready != 0;
#endif
}

} // namespace terminal

} // namespace basalt::wire::io
