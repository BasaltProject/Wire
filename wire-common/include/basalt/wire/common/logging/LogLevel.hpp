#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::wire::common {

enum class LogLevel : std::uint8_t {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
};

[[nodiscard]] constexpr std::string_view logLevelLabel(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
}

} // namespace basalt::wire::common
