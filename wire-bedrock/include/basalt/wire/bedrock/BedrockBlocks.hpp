#pragma once

#include <array>
#include <cstdint>

namespace basalt::wire::bedrock {

/// How many block states Java 26.2 has, which is what the generated table is indexed by.
inline constexpr std::size_t kJavaBlockStates = 32366;

/// The Bedrock block id of every Java block state, one table per protocol of the window. An id over
/// there is a position in the palette the client already holds, so nothing is sent. There is no
/// pattern to fold: the palette is ordered by the hash of the name and differs every version.
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIds260();
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIds2610();
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIds2620();
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIds2630();
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIds2640();

/// The table a client on this protocol reads its blocks from. A protocol this build does not know
/// gets the newest, which is the only answer that is ever right for an unknown newer client.
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIdsFor(int protocol);

/// The id a state out of range falls back to, which is what a state this build knows and that
/// palette does not would draw. Zero is air over there, so the gap is a hole rather than a wrong
/// block.
inline constexpr std::uint16_t kBedrockAir = 0;

[[nodiscard]] inline std::uint16_t bedrockBlockId(const std::int32_t javaState, const int protocol) {
    if (javaState < 0 || static_cast<std::size_t>(javaState) >= kJavaBlockStates) {
        return kBedrockAir;
    }
    return bedrockBlockIdsFor(protocol)[static_cast<std::size_t>(javaState)];
}

} // namespace basalt::wire::bedrock
