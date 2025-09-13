#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace basalt::wire::io {

/// Turns Ctrl+C and `kill` into an orderly shutdown instead of an abrupt death, which would cost
/// every player up to a whole save interval. SIGINT is the operator, SIGTERM is what systemd,
/// Docker and every supervisor send before SIGKILL.
///
/// The handler only sets a flag: anything else it could do risks deadlocking against the thread it
/// interrupted.
namespace shutdownSignal {

void install();

[[nodiscard]] bool isRequested() noexcept;

/// For tests and for a second shutdown attempt.
void reset() noexcept;

} // namespace shutdownSignal

/// Pushes whatever has been printed out to the terminal now. Standard output is line buffered and a
/// line editor redraws without ever writing a newline, so the prompt would otherwise sit in the
/// buffer and the operator would be typing into what looks like a dead terminal.
void flushConsole();

/// Blocks the calling thread.
void sleepFor(std::chrono::milliseconds duration);

/// Resident set size in bytes, or 0 when the platform will not say.
[[nodiscard]] std::uint64_t residentSetBytes();

/// Whether standard output is a terminal, which decides whether log lines get ANSI colour.
[[nodiscard]] bool isConsoleInteractive();

/// CPU time this process has used, or nothing where the platform cannot say.
///
/// Time used, not a percentage: a percentage only means something between two readings, and
/// deciding over what window is the caller's business, not this one's.
[[nodiscard]] std::optional<std::chrono::microseconds> processCpuTime();

/// Cores this process may run on, or nothing where the platform cannot say. For sizing worker
/// pools: a constant is what puts eight generation threads on a four-core machine.
[[nodiscard]] std::optional<unsigned int> cpuCoreCount();

/// Puts the terminal into raw mode, where a keypress arrives as it happens.
///
/// In canonical mode the terminal buffers a whole line and echoes what it does not understand, so an
/// arrow key prints `^[[A` instead of reaching the program: history editing needs this. False means
/// standard input is not a terminal, which is not a failure: the caller falls back to plain line
/// reading, which every one of those cases supports anyway.
namespace terminal {

[[nodiscard]] bool enableRawMode();

/// Puts the terminal back exactly as it was found. Safe to call even if raw mode was never entered.
void restoreMode();

/// One byte from standard input, or -1 at end of input. In raw mode this returns as soon as a key
/// is pressed.
[[nodiscard]] int readByte();

} // namespace terminal

} // namespace basalt::wire::io
