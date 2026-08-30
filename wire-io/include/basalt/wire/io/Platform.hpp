#pragma once

#include <chrono>
#include <cstdint>

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

} // namespace basalt::wire::io
