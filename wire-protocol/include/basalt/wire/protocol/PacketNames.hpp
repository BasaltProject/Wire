#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::protocol::packetNames {

/// Same order as the protocol states, so a cast to the connection's state is exact.
enum class Phase : std::uint8_t { kHandshake, kStatus, kLogin, kConfiguration, kPlay };

struct Address {
    Phase phase = Phase::kPlay;
    bool serverbound = false;
    std::int32_t id = 0;

    friend bool operator==(const Address&, const Address&) = default;
};

/// The name plugins use: `clientbound.set_title_text`, or with the phase when the name repeats across phases.
[[nodiscard]] std::string of(const Address& address);

/// One packet by any of its spellings: short, with the phase, a vanilla alias, or `clientbound.play.0x1f`.
[[nodiscard]] std::optional<Address> resolve(std::string_view name);

/// The phase named by a word such as `play`.
[[nodiscard]] std::optional<Phase> phaseOf(std::string_view word);

[[nodiscard]] std::vector<std::string> all();

} // namespace basalt::wire::protocol::packetNames
