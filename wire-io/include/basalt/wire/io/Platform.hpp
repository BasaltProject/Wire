#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace basalt::wire::io {

/// The handler only sets a flag: anything else risks deadlocking against the interrupted thread.
namespace shutdownSignal {

void install();

[[nodiscard]] bool isRequested() noexcept;

void reset() noexcept;

} // namespace shutdownSignal

void flushConsole();

void sleepFor(std::chrono::milliseconds duration);

[[nodiscard]] std::uint64_t residentSetBytes();

[[nodiscard]] bool isConsoleInteractive();

[[nodiscard]] std::optional<std::chrono::microseconds> processCpuTime();

[[nodiscard]] std::optional<unsigned int> cpuCoreCount();

/// Lets the thread give way to the others when every core is busy; background work like generation calls this.
void lowerCurrentThreadPriority();

namespace terminal {

[[nodiscard]] bool enableRawMode();

void restoreMode();

[[nodiscard]] int readByte();

[[nodiscard]] bool waitForInput(std::chrono::milliseconds timeout);

} // namespace terminal

} // namespace basalt::wire::io
