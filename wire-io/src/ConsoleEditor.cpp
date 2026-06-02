#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/io/ConsoleEditor.hpp>
#include <basalt/wire/io/ConsoleInput.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <optional>

namespace basalt::wire::io {
namespace {

constexpr int kTab = 9;
constexpr int kNewline = 10;
constexpr int kCarriageReturn = 13;
constexpr int kEscape = 27;
constexpr int kBackspace = 8;
constexpr int kDelete = 127;
constexpr int kBracket = 91;
constexpr int kUp = 65;
constexpr int kDown = 66;
constexpr int kRight = 67;
constexpr int kLeft = 68;
constexpr int kFirstPrintable = 32;
constexpr std::chrono::milliseconds kPoll{25};

bool startsWithIgnoringCase(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), text.begin(), [](char a, char b) {
               return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
           });
}

int readByteOrStop(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        if (terminal::waitForInput(kPoll))
            return terminal::readByte();
    }
    return -1;
}

std::mutex promptMutex;

std::optional<std::string> readEdited(const ConsoleEditorOptions& options, std::vector<std::string>& history,
                                      const std::stop_token& stop, std::function<void()>& redrawPrompt) {
    std::string line;
    std::size_t cursor = 0;
    std::size_t historyIndex = history.size();
    std::string draft;
    const auto redraw = [&line, &cursor] {
        std::fputs(("\r> " + line + "\033[K").c_str(), stdout);
        if (cursor < line.size())
            std::fputs(("\033[" + std::to_string(line.size() - cursor) + "D").c_str(), stdout);
        flushConsole();
    };
    {
        const std::lock_guard lock{promptMutex};
        redrawPrompt = redraw;
        redraw();
    }
    while (true) {
        const int byte = readByteOrStop(stop);
        int arrow = -1;
        if (byte == kEscape) {
            if (terminal::readByte() != kBracket)
                continue;
            arrow = terminal::readByte();
        }
        const std::lock_guard lock{promptMutex};
        if (byte == -1) {
            redrawPrompt = nullptr;
            return std::nullopt;
        }
        if (byte == kCarriageReturn || byte == kNewline) {
            redrawPrompt = nullptr;
            redraw();
            std::fputs("\r\n", stdout);
            flushConsole();
            return line;
        }
        if (byte == kTab) {
            if (!options.candidates)
                continue;
            Completion completion = complete(line, options.candidates(line));
            line = std::move(completion.line);
            cursor = line.size();
            if (!completion.options.empty()) {
                std::string listing = "\r\033[K";
                for (const std::string& option : completion.options)
                    listing += option + "  ";
                std::fputs((listing + "\r\n").c_str(), stdout);
            }
            redraw();
            continue;
        }
        if (byte == kBackspace || byte == kDelete) {
            if (cursor > 0) {
                line.erase(cursor - 1, 1);
                --cursor;
                redraw();
            }
            continue;
        }
        if (byte == kEscape) {
            if (arrow == kUp && historyIndex > 0) {
                if (historyIndex == history.size())
                    draft = line;
                line = history[--historyIndex];
                cursor = line.size();
                redraw();
            } else if (arrow == kDown && historyIndex < history.size()) {
                ++historyIndex;
                line = historyIndex == history.size() ? draft : history[historyIndex];
                cursor = line.size();
                redraw();
            } else if (arrow == kRight && cursor < line.size()) {
                ++cursor;
                redraw();
            } else if (arrow == kLeft && cursor > 0) {
                --cursor;
                redraw();
            }
            continue;
        }
        if (byte < kFirstPrintable || line.size() >= options.maxLineBytes)
            continue;
        line.insert(cursor, 1, static_cast<char>(byte));
        ++cursor;
        redraw();
    }
}

} // namespace

Completion complete(std::string_view line, std::vector<std::string> candidates) {
    const auto lastSpace = line.rfind(' ');
    const std::string_view head =
        lastSpace == std::string_view::npos ? std::string_view{} : line.substr(0, lastSpace + 1);
    const std::string_view partial = line.substr(head.size());
    std::erase_if(candidates,
                  [&](const std::string& candidate) { return !startsWithIgnoringCase(candidate, partial); });
    std::ranges::sort(candidates);
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    Completion result{std::string{line}, {}};
    if (candidates.empty())
        return result;
    if (candidates.size() == 1) {
        result.line = std::string{head} + candidates.front() + " ";
        return result;
    }
    std::string common = candidates.front();
    for (const std::string& candidate : candidates) {
        std::size_t same = 0;
        while (same < common.size() && same < candidate.size() &&
               std::tolower(static_cast<unsigned char>(common[same])) ==
                   std::tolower(static_cast<unsigned char>(candidate[same])))
            ++same;
        common.resize(same);
    }
    if (common.size() > partial.size())
        result.line = std::string{head} + common;
    result.options = std::move(candidates);
    return result;
}

void runConsoleEditor(const ConsoleEditorOptions& options, std::stop_token stop,
                      const std::function<void(std::string)>& onLine) {
    const bool editing = terminal::enableRawMode();
    std::function<void()> redrawPrompt;
    if (editing) {
        common::logOutput::setAroundConsoleLine([&redrawPrompt](const std::function<void()>& write) {
            const std::lock_guard lock{promptMutex};
            std::fputs("\r\033[K", stdout);
            write();
            if (redrawPrompt)
                redrawPrompt();
            flushConsole();
        });
    }
    ConsoleInput input{options.maxLineBytes};
    std::vector<std::string> history;
    while (!stop.stop_requested()) {
        std::optional<std::string> line;
        if (editing) {
            line = readEdited(options, history, stop, redrawPrompt);
        } else {
            auto read = input.readLine(stop);
            if (read)
                line = std::move(*read);
            else if (read.error() == ConsoleInputError::kTooLong)
                continue;
        }
        if (!line)
            break;
        if (!line->empty() && (history.empty() || history.back() != *line)) {
            history.push_back(*line);
            if (history.size() > options.maxHistory)
                history.erase(history.begin());
        }
        onLine(std::move(*line));
    }
    if (editing) {
        common::logOutput::setAroundConsoleLine({});
        terminal::restoreMode();
    }
}

} // namespace basalt::wire::io
