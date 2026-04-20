#pragma once

#include <basalt/wire/common/logging/LogLevel.hpp>

#include <functional>
#include <string>
#include <string_view>

namespace basalt::wire::common::logOutput {

void setUseColor(bool value) noexcept;

[[nodiscard]] bool useColor() noexcept;

void setAroundConsoleLine(std::function<void(const std::function<void()>&)> around);

/// Sinks receive the plain line, never the coloured one, so log files hold no escape codes.
void addSink(std::function<void(std::string_view)> sink);

void clearSinks();

void emit(LogLevel level, std::string_view timestamp, std::string_view name, std::string_view message);

void emitInPlace(LogLevel level, std::string_view timestamp, std::string_view name, std::string_view message);

void endInPlace();

} // namespace basalt::wire::common::logOutput
