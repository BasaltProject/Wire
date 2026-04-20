#pragma once

#include <array>
#include <cstdint>

namespace basalt::wire::bedrock {

inline constexpr std::size_t kJavaBlockStates = 35723;

/// An id over there is a position in the palette the client already holds, so nothing is sent.
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIds2630();
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIds2640();
[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIds2650();

[[nodiscard]] const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIdsFor(int protocol);

[[nodiscard]] inline std::uint16_t bedrockAirId(const int protocol) { return bedrockBlockIdsFor(protocol)[0]; }

/// A state out of range is drawn as air: a hole rather than a wrong block.
[[nodiscard]] inline std::uint16_t bedrockBlockId(const std::int32_t javaState, const int protocol) {
    if (javaState < 0 || static_cast<std::size_t>(javaState) >= kJavaBlockStates) {
        return bedrockAirId(protocol);
    }
    return bedrockBlockIdsFor(protocol)[static_cast<std::size_t>(javaState)];
}

[[nodiscard]] std::uint16_t itemFrameBlockId(bool glowing, int face, int protocol);

} // namespace basalt::wire::bedrock
