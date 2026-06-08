#pragma once

#include <basalt/wire/plugins/PluginPackets.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::plugins::packetNaming {

/// The name plugins use: `clientbound.set_title_text`, or with the phase when the name repeats across phases.
[[nodiscard]] std::string of(const PacketAddress& address);

/// One packet by any of its spellings: short, with the phase, a vanilla alias, or `clientbound.play.0x1f`.
[[nodiscard]] std::optional<PacketAddress> resolve(std::string_view name);

/// Like `resolve`, plus `*`, `serverbound.*` and `clientbound.play.*`.
[[nodiscard]] std::optional<PacketScope> scopeOf(std::string_view name);

[[nodiscard]] std::vector<std::string> all();

} // namespace basalt::wire::plugins::packetNaming
