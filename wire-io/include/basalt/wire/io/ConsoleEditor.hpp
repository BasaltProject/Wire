#pragma once

#include <cstddef>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::io {

struct Completion {
    std::string line;
    /// What to list for the user when the candidates still differ after the line was extended as far as it can go.
    std::vector<std::string> options;
};

/// Completes the last word of `line`: the whole word when one candidate fits, their common prefix otherwise.
[[nodiscard]] Completion complete(std::string_view line, std::vector<std::string> candidates);

struct ConsoleEditorOptions {
    /// Candidates for the last word of the line typed so far.
    std::function<std::vector<std::string>(std::string_view line)> candidates;
    std::size_t maxLineBytes = 4096;
    std::size_t maxHistory = 100;
};

/// Reads console lines until `stop` or the end of input; a terminal gets editing, history and Tab completion.
void runConsoleEditor(const ConsoleEditorOptions& options, std::stop_token stop,
                      const std::function<void(std::string)>& onLine);

} // namespace basalt::wire::io
