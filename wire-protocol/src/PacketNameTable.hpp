#pragma once

#include <basalt/wire/protocol/PacketNames.hpp>

#include <span>
#include <string_view>

namespace basalt::wire::protocol::packetNames {

struct Row {
    Phase phase;
    bool serverbound;
    std::int32_t id;
    std::string_view name;
};

[[nodiscard]] std::span<const Row> rows();

[[nodiscard]] std::span<const Row> aliases();

} // namespace basalt::wire::protocol::packetNames
