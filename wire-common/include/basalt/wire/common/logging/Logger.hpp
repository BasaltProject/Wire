#pragma once

#include <basalt/wire/common/logging/LogLevel.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace basalt::wire::common {

/// A minimal, dependency-free leveled logger writing `[yyyy-MM-dd HH:mm:ss] [LEVEL] [name]: message`
/// to stdout, in local time.
///
/// Message producers are callables, so a call filtered out by minLevel builds no string.
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

    /// One info line that the next call draws over, for a step whose only news is how far along it
    /// is. Ended with `endProgress`, which is what puts the console back on a fresh line.
    template <typename Message>
    void progress(Message&& message) const {
        write(LogLevel::Info, std::forward<Message>(message), true);
    }

    void endProgress() const { logOutput::endInPlace(); }

    void setMinLevel(LogLevel level) noexcept { minLevel_ = level; }

    [[nodiscard]] LogLevel minLevel() const noexcept { return minLevel_; }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /// Applied to every Logger created after it is set; existing instances keep their own level.
    static void setDefaultMinLevel(LogLevel level) noexcept;

    [[nodiscard]] static LogLevel defaultMinLevel() noexcept;

    [[nodiscard]] static std::string timestamp();

    /// A stored instant in the same shape as the timestamp on every log line, so a date inside a
    /// message reads like the one in front of it instead of as a number nobody can decode.
    [[nodiscard]] static std::string formatEpochSeconds(std::int64_t epochSeconds);

    /// Just the local calendar date, `yyyy-MM-dd`. It has to be local for the same reason the
    /// timestamps are: an archive written at one in the morning belongs to the day the operator
    /// thinks it does.
    [[nodiscard]] static std::string formatLocalDate(std::int64_t epochSeconds);

private:
    /// The message is built inside each branch rather than once above them so that a producer
    /// answering a `const char*` still reaches the sink without a string being made for it.
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
