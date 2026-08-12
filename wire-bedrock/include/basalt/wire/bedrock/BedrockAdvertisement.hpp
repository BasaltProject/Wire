#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::wire::bedrock {

struct Advertisement {
    std::string motd;
    std::string subMotd;
    std::string gameVersion;
    std::size_t online = 0;
    std::size_t max = 0;
    std::uint64_t guid = 0;
    std::uint16_t port = 0;
};

/// The title and the line under it, split at the first newline; the second is empty when there is none.
[[nodiscard]] std::pair<std::string, std::string> splitMotd(std::string_view text);

/// The unconnected-pong line a client reads in its server list; semicolons in the texts are blanked out.
[[nodiscard]] std::string advertisementLine(const Advertisement& status);

} // namespace basalt::wire::bedrock
