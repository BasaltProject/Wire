#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <cstdint>
#include <functional>
#include <string>

namespace basalt::wire::config {

struct BasicFacts {
    std::string versionName;
    std::int32_t protocolVersion = 0;
    std::function<std::int32_t()> online;
    std::function<std::int32_t()> maxPlayers;
};

/// Registers the `%basalt_server_*%` and `%basalt_player_*%` names a server list ping can answer.
void registerBasicPlaceholders(common::placeholder::PlaceholderRegistry& registry, BasicFacts facts);

} // namespace basalt::wire::config
