#pragma once

#include <array>
#include <cstdint>

namespace basalt::wire::bedrock {

/// How many block states Java 26.3 has, which is what the generated table is indexed by.
inline constexpr std::size_t kJavaBlockStates = 35723;

/// The Bedrock block id of every Java block state, one table per protocol of the window. An id over
/// there is a position in the palette the client already holds, so nothing is sent. There is no
/// pattern to fold: the palette is ordered by the hash of the name and differs every version.
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIds2630();
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIds2640();
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIds2650();

/// The table a client on this protocol reads its blocks from. A protocol this build does not know
/// gets the newest, which is the only answer that is ever right for an unknown newer client.
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIdsFor(int protocol);

/// Air in that protocol's palette, which is where Java state zero lands. A palette is sorted by the
/// hash of the name, so air has a different number in each one.
[[nodiscard]] inline std::uint16_t bedrockAirId(const int protocol) { return bedrockBlockIdsFor(protocol)[0]; }

/// A state out of range is drawn as air: a hole rather than a wrong block.
[[nodiscard]] inline std::uint16_t bedrockBlockId(const std::int32_t javaState, const int protocol) {
    if (javaState < 0 || static_cast<std::size_t>(javaState) >= kJavaBlockStates) {
        return bedrockAirId(protocol);
    }
    return bedrockBlockIdsFor(protocol)[static_cast<std::size_t>(javaState)];
}

/// Where an item frame hangs, which is a block on that edition rather than the entity it is here.
/// `face` is down, up, north, south, west, east, the order both editions number the six by.
[[nodiscard]] std::uint16_t itemFrameBlockId(bool glowing, int face, int protocol);

} // namespace basalt::wire::bedrock
