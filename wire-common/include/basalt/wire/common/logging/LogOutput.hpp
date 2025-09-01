#pragma once

#include <basalt/wire/common/logging/LogLevel.hpp>

#include <functional>
#include <string>
#include <string_view>

namespace basalt::wire::common::logOutput {

/// Where log lines actually go. The console always gets them; anything else (a log file, a plugin
/// hook) registers through addSink.
///
/// Sinks are a callback rather than a direct file write because wire-common has no dependencies by
/// design, so it cannot reach the filesystem itself. The file sink lives in wire-io, which can, and
/// installs itself here at startup. Sinks receive the plain line, never the coloured one: escape
/// codes written into a log file are noise every later reader has to strip.

/// Whether the console gets ANSI colour. Decided at startup.
void setUseColor(bool value) noexcept;

[[nodiscard]] bool useColor() noexcept;

/// Wraps every console line, for whoever else is drawing on the same terminal. The console command
/// editor keeps a prompt on the bottom line and redraws it in place; a log line written straight
/// over it leaves the operator typing into wreckage.
void setAroundConsoleLine(std::function<void(const std::function<void()>&)> around);

/// Registers an extra destination. Not synchronised: sinks are installed once during startup,
/// before anything else is logging, and the list is only read afterwards.
void addSink(std::function<void(std::string_view)> sink);

/// Drops every registered sink, so a shut-down file sink cannot be written to afterwards.
void clearSinks();

void emit(LogLevel level, std::string_view timestamp, std::string_view name, std::string_view message);

} // namespace basalt::wire::common::logOutput
