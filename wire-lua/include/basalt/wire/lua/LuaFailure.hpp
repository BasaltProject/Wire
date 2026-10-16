#pragma once

#include <string>
#include <string_view>

namespace basalt::wire::lua {
struct LuaFailure {
    std::string plugin;
    std::string message;

    [[nodiscard]] std::string report() const { return "[" + plugin + "] " + message; }

    [[nodiscard]] bool outOfBudget() const;

    [[nodiscard]] bool outOfMemory() const;
};

namespace failureMessages {
inline constexpr std::string_view kBudget = "instruction budget exhausted";
inline constexpr std::string_view kInterrupted = "interrupted";
inline constexpr std::string_view kMemory = "not enough memory";
} // namespace failureMessages
} // namespace basalt::wire::lua
