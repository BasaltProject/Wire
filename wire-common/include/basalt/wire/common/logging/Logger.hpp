#pragma once

#include <basalt/wire/common/logging/LogLevel.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace basalt::wire::common {

class Logger {
public:
    explicit Logger(std::string name) : name_(std::move(name)), minLevel_(defaultMinLevel()) {}

    template <typename Message>
    void trace(Message&& message) const {
        log(LogLevel::Trace, std::forward<Message>(message));
    }

    template <typename Message>
    void debug(Message&& message) const {
        log(LogLevel::Debug, std::forward<Message>(message));
    }

    template <typename Message>
    void info(Message&& message) const {
        log(LogLevel::Info, std::forward<Message>(message));
    }

    template <typename Message>
    void warn(Message&& message) const {
        log(LogLevel::Warn, std::forward<Message>(message));
    }

    template <typename Message>
    void error(Message&& message) const {
        log(LogLevel::Error, std::forward<Message>(message));
    }

    template <typename Message>
    void log(LogLevel level, Message&& message) const {
        write(level, std::forward<Message>(message), false);
    }

    template <typename Message>
    void progress(Message&& message) const {
        write(LogLevel::Info, std::forward<Message>(message), true);
    }

    void endProgress() const { logOutput::endInPlace(); }

    void setMinLevel(LogLevel level) noexcept { minLevel_ = level; }

    [[nodiscard]] LogLevel minLevel() const noexcept { return minLevel_; }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    static void setDefaultMinLevel(LogLevel level) noexcept;

    [[nodiscard]] static LogLevel defaultMinLevel() noexcept;

    [[nodiscard]] static std::string timestamp();

    [[nodiscard]] static std::string formatEpochSeconds(std::int64_t epochSeconds);

    [[nodiscard]] static std::string formatLocalDate(std::int64_t epochSeconds);

private:
    template <typename Message>
    void write(LogLevel level, Message&& message, bool inPlace) const {
        if (static_cast<std::uint8_t>(level) < static_cast<std::uint8_t>(minLevel_)) {
            return;
        }
        if (inPlace) {
            logOutput::emitInPlace(level, timestamp(), name_, std::forward<Message>(message)());
        } else {
            logOutput::emit(level, timestamp(), name_, std::forward<Message>(message)());
        }
    }

    std::string name_;
    LogLevel minLevel_;
};

} // namespace basalt::wire::common
