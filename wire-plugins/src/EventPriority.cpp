#include <basalt/wire/plugins/EventPriority.hpp>

#include <algorithm>
#include <array>
#include <cctype>

namespace basalt::wire::plugins {
namespace {

constexpr std::array<std::pair<EventPriority, std::string_view>, 5> kNames = {{
    {EventPriority::First, "FIRST"},
    {EventPriority::Early, "EARLY"},
    {EventPriority::Normal, "NORMAL"},
    {EventPriority::Late, "LATE"},
    {EventPriority::Last, "LAST"},
}};

[[nodiscard]] bool equalsIgnoringCase(std::string_view left, std::string_view right) {
    return left.size() == right.size() && std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
               return std::tolower(a) == std::tolower(b);
           });
}

} // namespace

std::string_view priorityName(EventPriority priority) {
    const auto found = std::ranges::find(kNames, priority, &std::pair<EventPriority, std::string_view>::first);
    return found == kNames.end() ? std::string_view{"NORMAL"} : found->second;
}

std::optional<EventPriority> priorityOf(std::string_view name) {
    for (const auto& [priority, spelled] : kNames)
        if (equalsIgnoringCase(spelled, name))
            return priority;
    return std::nullopt;
}

} // namespace basalt::wire::plugins
