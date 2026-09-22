#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::config {

class PingRequest;

struct BasicFacts {
    std::string versionName;
    std::int32_t protocolVersion = 0;
    std::function<std::int32_t()> online;
    std::function<std::int32_t()> maxPlayers;
};

/// `version`, `protocol`, `online` and `max_players` of `%basalt_server_*%`; nothing for any other name.
[[nodiscard]] std::optional<common::placeholder::PlaceholderValue> basicServerValue(const BasicFacts& facts,
                                                                                    std::string_view params);

/// `protocol`, `host`, `ip`, `known` and `name` of the pinging player; nothing when the answer is unknown.
[[nodiscard]] std::optional<common::placeholder::PlaceholderValue> pingValue(const PingRequest& ping,
                                                                             std::string_view params);

/// Registers the `%basalt_server_*%` and `%basalt_player_*%` names a server list ping can answer.
void registerBasicPlaceholders(common::placeholder::PlaceholderRegistry& registry, BasicFacts facts);

} // namespace basalt::wire::config
