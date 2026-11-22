#include "Scenario.hpp"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::wasm::trace {
namespace {
std::vector<std::pair<std::string_view, ScenarioVerb>>& areaVerbs() {
    static std::vector<std::pair<std::string_view, ScenarioVerb>> verbs;
    return verbs;
}

[[nodiscard]] std::string_view trimmed(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] std::string_view takeWord(std::string_view& rest) {
    rest = trimmed(rest);
    const std::size_t end = rest.find(' ');
    const std::string_view word = rest.substr(0, end);
    rest = end == std::string_view::npos ? std::string_view{} : trimmed(rest.substr(end));
    return word;
}

[[nodiscard]] bool toInt(std::string_view text, std::int64_t& out) {
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t used = 0;
        out = std::stoll(std::string{text}, &used);
        return used == text.size();
    } catch (...) {
        return false;
    }
}
} // namespace

ScenarioVerbRegistrar::ScenarioVerbRegistrar(std::string_view verb, ScenarioVerb run) {
    areaVerbs().emplace_back(verb, run);
}

bool runScenario(World& world, std::istream& input) {
    std::string raw;
    while (std::getline(input, raw)) {
        const std::string_view line = trimmed(raw);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        emitLine("# " + std::string{line});
        std::string_view rest = line;
        const std::string_view verb = takeWord(rest);
        if (verb == "enable") {
            world.enable();
        } else if (verb == "disable") {
            world.disable();
        } else if (verb == "join") {
            world.join(takeWord(rest));
        } else if (verb == "leave") {
            world.leave(takeWord(rest));
        } else if (verb == "command") {
            const std::string_view source = takeWord(rest);
            world.runCommand(source, rest);
        } else if (verb == "tick") {
            std::int64_t count = 0;
            if (!toInt(takeWord(rest), count) || count < 0) {
                std::fprintf(stderr, "bad tick count in: %s\n", raw.c_str());
                return false;
            }
            world.advance(count);
        } else if (verb == "click") {
            std::int64_t menu = 0;
            std::int64_t slot = 0;
            if (!toInt(takeWord(rest), menu) || !toInt(takeWord(rest), slot)) {
                std::fprintf(stderr, "bad click in: %s\n", raw.c_str());
                return false;
            }
            world.click(static_cast<std::int32_t>(menu), static_cast<std::int32_t>(slot), takeWord(rest));
        } else if (verb == "placeholder") {
            const std::string_view name = takeWord(rest);
            world.placeholder(name, takeWord(rest));
        } else {
            bool known = false;
            for (const auto& [name, run] : areaVerbs()) {
                if (name == verb) {
                    known = true;
                    if (!run(world, rest)) {
                        std::fprintf(stderr, "bad scenario command: %s\n", raw.c_str());
                        return false;
                    }
                    break;
                }
            }
            if (!known) {
                std::fprintf(stderr, "unknown scenario command: %s\n", raw.c_str());
                return false;
            }
        }
        world.drain();
    }
    return true;
}

} // namespace basalt::wire::wasm::trace
