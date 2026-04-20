#pragma once

#include <chrono>
#include <expected>
#include <functional>
#include <stop_token>
#include <string>

namespace basalt::wire::io {

enum class ConsoleInputError { kEnd, kStopped, kTooLong };

/// Single reader; an oversized line is discarded through its delimiter before returning an error.
class ConsoleInput {
public:
    using Wait = std::function<bool(std::chrono::milliseconds)>;
    using Read = std::function<int()>;
    explicit ConsoleInput(std::size_t maxLineBytes = 4096);
    ConsoleInput(std::size_t maxLineBytes, Wait wait, Read read);
    [[nodiscard]] std::expected<std::string, ConsoleInputError> readLine(std::stop_token stop);

private:
    std::size_t limit_;
    Wait wait_;
    Read read_;
    bool skipLineFeed_ = false;
};

} // namespace basalt::wire::io
