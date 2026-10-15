#include <basalt/wire/io/ConsoleInput.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <algorithm>
#include <utility>

namespace basalt::wire::io {

ConsoleInput::ConsoleInput(std::size_t maxLineBytes)
    : ConsoleInput(maxLineBytes, terminal::waitForInput, terminal::readByte) {}

ConsoleInput::ConsoleInput(std::size_t maxLineBytes, Wait wait, Read read)
    : limit_(maxLineBytes), wait_(std::move(wait)), read_(std::move(read)) {}

std::expected<std::string, ConsoleInputError> ConsoleInput::readLine(std::stop_token stop) {
    std::string line;
    line.reserve(std::min(limit_, std::size_t{128}));
    bool oversized = false;
    while (!stop.stop_requested()) {
        if (!wait_(std::chrono::milliseconds{25}))
            continue;
        if (stop.stop_requested())
            break;
        const int byte = read_();
        if (byte < 0) {
            if (oversized)
                return std::unexpected(ConsoleInputError::kTooLong);
            if (!line.empty())
                return line;
            return std::unexpected(ConsoleInputError::kEnd);
        }
        if (skipLineFeed_) {
            skipLineFeed_ = false;
            if (byte == '\n')
                continue;
        }
        if (byte == '\r' || byte == '\n') {
            skipLineFeed_ = byte == '\r';
            if (oversized)
                return std::unexpected(ConsoleInputError::kTooLong);
            return line;
        }
        if (line.size() < limit_)
            line.push_back(static_cast<char>(byte));
        else
            oversized = true;
    }
    return std::unexpected(ConsoleInputError::kStopped);
}

} // namespace basalt::wire::io
