#include <basalt/wire/common/logging/LogOutput.hpp>

#include <atomic>
#include <cstdio>
#include <utility>
#include <vector>

namespace basalt::wire::common::logOutput {
namespace {

constexpr std::string_view kReset = "\x1B[0m";
constexpr std::string_view kDim = "\x1B[2m";
constexpr std::string_view kRed = "\x1B[31m";
constexpr std::string_view kGreen = "\x1B[32m";
constexpr std::string_view kYellow = "\x1B[33m";
constexpr std::string_view kCyan = "\x1B[36m";

bool gUseColor = false;
std::function<void(const std::function<void()>&)> gAroundConsoleLine;

/// Read by whichever thread logs next, so not a plain bool.
std::atomic<bool> gInPlace{false};

constexpr std::string_view kWipeLine = "\r\x1B[K";

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

namespace {

struct Rendered {
    std::string plain;
    std::string coloured;
};

Rendered render(LogLevel level, std::string_view timestamp, std::string_view name, std::string_view message) {
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

    return Rendered{.plain = std::move(plain), .coloured = std::move(line)};
}

} // namespace

void emit(LogLevel level, std::string_view timestamp, std::string_view name, std::string_view message) {
    const Rendered rendered = render(level, timestamp, name, message);
    const std::string_view console = gUseColor ? std::string_view{rendered.coloured} : std::string_view{rendered.plain};
    const auto write = [&console]() {
        if (gInPlace.exchange(false, std::memory_order_acq_rel) && gUseColor) {
            std::fwrite(kWipeLine.data(), 1, kWipeLine.size(), stdout);
        }
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
        sink(rendered.plain);
    }
}

void emitInPlace(LogLevel level, std::string_view timestamp, std::string_view name, std::string_view message) {
    const bool first = !gInPlace.load(std::memory_order_acquire);
    if (!gUseColor) {
        if (first) {
            emit(level, timestamp, name, message);
            gInPlace.store(true, std::memory_order_release);
        }
        return;
    }
    const Rendered rendered = render(level, timestamp, name, message);
    std::fwrite(kWipeLine.data(), 1, kWipeLine.size(), stdout);
    std::fwrite(rendered.coloured.data(), 1, rendered.coloured.size(), stdout);
    std::fflush(stdout);
    gInPlace.store(true, std::memory_order_release);
    if (first) {
        for (const auto& sink : sinks()) {
            sink(rendered.plain);
        }
    }
}

void endInPlace() {
    if (!gInPlace.exchange(false, std::memory_order_acq_rel) || !gUseColor) {
        return;
    }
    std::fwrite(kWipeLine.data(), 1, kWipeLine.size(), stdout);
    std::fflush(stdout);
}

} // namespace basalt::wire::common::logOutput
