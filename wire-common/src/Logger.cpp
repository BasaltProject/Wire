#include <basalt/wire/common/logging/Logger.hpp>

#include <array>
#include <chrono>
#include <ctime>

namespace basalt::wire::common {
namespace {

LogLevel gDefaultMinLevel = LogLevel::Info;

// Local time, not UTC: an operator reads their own console against their own clock, and a log two
// hours off their wall clock is a tax paid on every line. It is also what vanilla does. The zone is
// resolved per call rather than cached so a server running across a daylight-saving change keeps
// telling the truth.
std::tm localTime(std::int64_t epochSeconds) {
    const auto raw = static_cast<std::time_t>(epochSeconds);
    std::tm out{};
#if defined(_WIN32)
    localtime_s(&out, &raw);
#else
    localtime_r(&raw, &out);
#endif
    return out;
}

std::string format(std::int64_t epochSeconds) {
    const std::tm local = localTime(epochSeconds);
    std::array<char, 32> buffer{};
    const std::size_t written = std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M:%S", &local);
    return std::string{buffer.data(), written};
}

std::int64_t nowEpochSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace

void Logger::setDefaultMinLevel(LogLevel level) noexcept { gDefaultMinLevel = level; }

LogLevel Logger::defaultMinLevel() noexcept { return gDefaultMinLevel; }

std::string Logger::timestamp() { return format(nowEpochSeconds()); }

std::string Logger::formatEpochSeconds(std::int64_t epochSeconds) { return format(epochSeconds); }

std::string Logger::formatLocalDate(std::int64_t epochSeconds) { return format(epochSeconds).substr(0, 10); }

} // namespace basalt::wire::common
