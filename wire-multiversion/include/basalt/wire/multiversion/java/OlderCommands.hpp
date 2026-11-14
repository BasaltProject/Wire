#pragma once

#include <basalt/wire/protocol/play/CommandPackets.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {

/// The command tree in the shape an older client reads, each parser one that client has.
namespace olderCommands {

/// The native id the finished tree travels under, which no link reshapes again.
inline constexpr std::int32_t kTranslatedCommands = 0x7F10;

/// `missing` says whether a registry a parser names is one the client lacks.
[[nodiscard]] std::vector<std::uint8_t> write(const protocol::play::CommandsPacket& commands, std::int32_t farEnd,
                                              const std::function<bool(const std::string&)>& missing);

} // namespace olderCommands

} // namespace basalt::wire::multiversion::java
