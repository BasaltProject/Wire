#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/common/logging/Panic.hpp>

#include <array>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>

#if defined(__APPLE__) || defined(__linux__)
#include <execinfo.h>
#include <pthread.h>
#include <unistd.h>
#endif

namespace basalt::wire::common::panic {
namespace {

std::string productName;
std::string productVersion;
std::string reportAt;

std::atomic<bool> reporting{false};

constexpr std::string_view kRed = "\033[1;31m";
constexpr std::string_view kReset = "\033[0m";
constexpr std::string_view kRule = "----------------------------------------------------------------";

/// Uses write, not std::cerr: this runs in a signal handler where the stream's lock may be held.
void say(std::string_view text) {
#if defined(__APPLE__) || defined(__linux__)
    static_cast<void>(::write(STDERR_FILENO, text.data(), text.size()));
#else
    std::fwrite(text.data(), 1, text.size(), stderr);
#endif
}

void sayLine(std::string_view text) {
    say(text);
    say("\n");
}

void sayHex(std::uintptr_t value) {
    std::array<char, 18> digits{};
    digits[0] = '0';
    digits[1] = 'x';
    std::size_t at = digits.size();
    do {
        digits[--at < 2 ? 2 : at] = "0123456789abcdef"[value & 0xF];
        value >>= 4U;
    } while (value != 0 && at > 2);
    say("0x");
    say(std::string_view{digits.data() + at, digits.size() - at});
}

[[nodiscard]] bool colour() { return logOutput::useColor(); }

void openReport(std::string_view title) {
    say("\n");
    if (colour()) {
        say(kRed);
    }
    sayLine(kRule);
    say("  ");
    sayLine(title);
    sayLine(kRule);
    if (colour()) {
        say(kReset);
    }
    if (!productName.empty()) {
        say("  ");
        say(productName);
        say(" ");
        sayLine(productVersion);
    }
}

void closeReport() {
    if (colour()) {
        say(kRed);
    }
    sayLine(kRule);
    if (colour()) {
        say(kReset);
    }
    if (reportAt.empty()) {
        sayLine("  Please report this with the lines above and what you were doing.");
    } else {
        say("  Please report this at ");
        sayLine(reportAt);
        sayLine("  with the lines above and what you were doing.");
    }
    say("\n");
}

void sayBacktrace(int skip) {
#if defined(__APPLE__) || defined(__linux__)
    std::array<void*, 64> frames{};
    const int taken = ::backtrace(frames.data(), static_cast<int>(frames.size()));
    const int from = taken > skip ? skip : 0;
    sayLine("  Stack, innermost first. Names are mangled: pipe them through c++filt.");
    // backtrace_symbols_fd does not allocate; backtrace_symbols would take a lock the crashing thread may hold.
    ::backtrace_symbols_fd(frames.data() + from, taken - from, STDERR_FILENO);
#else
    static_cast<void>(skip);
    sayLine("  Stack: not available on this platform.");
#endif
}

/// Which thread died, so a crash in a connection is not read as a crash in the tick.
void sayThread() {
#if defined(__APPLE__) || defined(__linux__)
    std::array<char, 64> name{};
    say("  Thread: ");
    sayHex(reinterpret_cast<std::uintptr_t>(::pthread_self()));
    if (::pthread_getname_np(::pthread_self(), name.data(), name.size()) == 0 && name[0] != '\0') {
        say(" (");
        say(std::string_view{name.data()});
        say(")");
    }
    say("\n");
#endif
}

[[nodiscard]] std::string_view signalName(int number) {
    switch (number) {
    case SIGSEGV:
        return "SIGSEGV: a read or write through a pointer that does not point anywhere";
#if defined(SIGBUS)
    case SIGBUS:
        return "SIGBUS: a read or write the machine cannot do at that address";
#endif
    case SIGILL:
        return "SIGILL: an instruction the machine does not have";
    case SIGFPE:
        return "SIGFPE: an arithmetic fault";
    case SIGABRT:
        return "SIGABRT: the process gave up on itself";
    default:
        return "a fatal signal";
    }
}

void die(int number) {
    std::signal(number, SIG_DFL);
    std::raise(number);
}

void describe(int number, const void* address, bool addressKnown) {
    openReport("The server crashed");
    say("  ");
    sayLine(signalName(number));
    if (addressKnown) {
        say("  At: ");
        sayHex(reinterpret_cast<std::uintptr_t>(address));
        say("\n");
    }
    sayThread();
    sayBacktrace(3);
    closeReport();
}

#if defined(__APPLE__) || defined(__linux__)
void onSignal(int number, siginfo_t* info, void*) {
    bool expected = false;
    if (!reporting.compare_exchange_strong(expected, true)) {
        // A second thread crashing mid-report dies quietly rather than interleave two reports.
        die(number);
        return;
    }
    describe(number, info == nullptr ? nullptr : info->si_addr, info != nullptr);
    die(number);
}
#else
void onSignal(int number) {
    bool expected = false;
    if (!reporting.compare_exchange_strong(expected, true)) {
        die(number);
        return;
    }
    describe(number, nullptr, false);
    die(number);
}
#endif

void onTerminate() {
    bool expected = false;
    if (reporting.compare_exchange_strong(expected, true)) {
        openReport("The server stopped on an error nobody handled");
        if (std::exception_ptr held = std::current_exception(); held != nullptr) {
            try {
                std::rethrow_exception(held);
            } catch (const std::exception& error) {
                say("  ");
                sayLine(error.what());
            } catch (...) {
                sayLine("  An exception that is not a std::exception.");
            }
        }
        sayThread();
        sayBacktrace(2);
        closeReport();
    }
    std::abort();
}

} // namespace

void install(std::string name, std::string version, std::string reportUrl) {
    productName = std::move(name);
    productVersion = std::move(version);
    reportAt = std::move(reportUrl);
    constexpr int kFatalSignals[] = {SIGSEGV,
#if defined(SIGBUS)
                                     SIGBUS,
#endif
                                     SIGILL, SIGFPE, SIGABRT};
    for (const int number : kFatalSignals) {
#if defined(__APPLE__) || defined(__linux__)
        struct sigaction action{};
        action.sa_sigaction = onSignal;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        static_cast<void>(::sigaction(number, &action, nullptr));
#else
        std::signal(number, onSignal);
#endif
    }
    std::set_terminate(onTerminate);
}

void report(std::string_view title, std::string_view detail) {
    openReport(title);
    say("  ");
    sayLine(detail);
    closeReport();
}

} // namespace basalt::wire::common::panic
