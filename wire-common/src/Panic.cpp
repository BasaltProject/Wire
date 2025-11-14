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
/// Held as a string rather than formatted on the spot: the handler may not allocate, so whatever it
/// prints has to exist before the crash does.
std::string reportAt;

/// Already inside a report: a handler that faults again would recurse until the stack runs out.
std::atomic<bool> reporting{false};

constexpr std::string_view kRed = "\033[1;31m";
constexpr std::string_view kReset = "\033[0m";
constexpr std::string_view kRule = "----------------------------------------------------------------";

/// Written with `write`, not `std::cerr`: this runs from a signal handler, where the stream's own
/// lock may be held by the thread that just died.
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

/// Hex by hand: `printf` takes a lock, and this runs where a lock may already be held.
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

/// The frames of whoever is crashing, as far as the platform will give them.
///
/// `skip` drops this module's own frames, which are never what anyone is looking for.
void sayBacktrace(int skip) {
#if defined(__APPLE__) || defined(__linux__)
    std::array<void*, 64> frames{};
    const int taken = ::backtrace(frames.data(), static_cast<int>(frames.size()));
    const int from = taken > skip ? skip : 0;
    sayLine("  Stack, innermost first. Names are mangled: pipe them through c++filt.");
    // `backtrace_symbols_fd` writes straight to the descriptor precisely so it can be used here:
    // the allocating `backtrace_symbols` would take a lock the crashing thread may already hold,
    // and demangling allocates too.
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
    case SIGBUS:
        return "SIGBUS: a read or write the machine cannot do at that address";
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
    // Back to the default handler and round again, so the exit status and the platform's own crash
    // report are what they would have been without any of this.
    std::signal(number, SIG_DFL);
    std::raise(number);
}

void describe(int number, const void* address, bool addressKnown) {
    openReport("The server crashed");
    say("  ");
    sayLine(signalName(number));
    // Printed even when it is zero: a fault at zero is a null pointer, which is the single most
    // useful thing the report can say.
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
        // A second thread crashing while the first is still printing: let it die quietly rather
        // than interleave two reports.
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
    for (const int number : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT}) {
#if defined(__APPLE__) || defined(__linux__)
        // `sigaction` rather than `signal` for one reason: it carries the address that faulted,
        // which is the difference between "it crashed" and "it read through a null pointer".
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
