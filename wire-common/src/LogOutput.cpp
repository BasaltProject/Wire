#include <basalt/wire/common/logging/LogOutput.hpp>

#include <cstdio>
#include <vector>

namespace basalt::wire::common::logOutput {
namespace {

// Written as an escape, not a literal ESC byte: a raw control character in source survives neither
// editors nor copy-paste reliably.
constexpr std::string_view kReset = "\x1B[0m";
constexpr std::string_view kDim = "\x1B[2m";
constexpr std::string_view kRed = "\x1B[31m";
constexpr std::string_view kGreen = "\x1B[32m";
constexpr std::string_view kYellow = "\x1B[33m";
constexpr std::string_view kCyan = "\x1B[36m";

bool gUseColor = false;
std::function<void(const std::function<void()>&)> gAroundConsoleLine;

std::vector<std::function<void(std::string_view)>>& sinks() {
    static std::vector<std::function<void(std::string_view)>> instance;
    return instance;
}

constexpr std::string_view levelColor(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace:
        return kDim;
    case LogLevel::Debug:
        return kCyan;
    case LogLevel::Info:
        return kGreen;
    case LogLevel::Warn:
        return kYellow;
    case LogLevel::Error:
        return kRed;
    }
    return kGreen;
}

// Only warnings and errors tint the message itself; colouring every info line makes a busy console
// harder to scan, not easier.
constexpr std::string_view messageColor(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Warn:
        return kYellow;
    case LogLevel::Error:
        return kRed;
    default:
        return "";
    }
}

} // namespace

void setUseColor(bool value) noexcept { gUseColor = value; }

bool useColor() noexcept { return gUseColor; }

void setAroundConsoleLine(std::function<void(const std::function<void()>&)> around) {
    gAroundConsoleLine = std::move(around);
}

void addSink(std::function<void(std::string_view)> sink) { sinks().push_back(std::move(sink)); }

void clearSinks() { sinks().clear(); }

void emit(LogLevel level, std::string_view timestamp, std::string_view name, std::string_view message) {
    std::string plain;
    plain.reserve(timestamp.size() + name.size() + message.size() + 24);
    plain += '[';
    plain += timestamp;
    plain += "] [";
    plain += logLevelLabel(level);
    plain += "] [";
    plain += name;
    plain += "]: ";
    plain += message;

    std::string line;
    if (gUseColor) {
        line.reserve(plain.size() + 48);
        line += kDim;
        line += '[';
        line += timestamp;
        line += ']';
        line += kReset;
        line += ' ';
        line += levelColor(level);
        line += '[';
        line += logLevelLabel(level);
        line += ']';
        line += kReset;
        line += ' ';
        line += kDim;
        line += '[';
        line += name;
        line += "]:";
        line += kReset;
        line += ' ';
        line += messageColor(level);
        line += message;
        line += kReset;
    }

    const std::string_view console = gUseColor ? std::string_view{line} : std::string_view{plain};
    // Flushed on every line: stdout is fully buffered when it is a pipe, and a server whose log
    // only appears when the buffer fills is useless under systemd, Docker or a hosting panel.
    const auto write = [&console]() {
        std::fwrite(console.data(), 1, console.size(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    };
    if (gAroundConsoleLine) {
        gAroundConsoleLine(write);
    } else {
        write();
    }

    for (const auto& sink : sinks()) {
        sink(plain);
    }
}

} // namespace basalt::wire::common::logOutput
