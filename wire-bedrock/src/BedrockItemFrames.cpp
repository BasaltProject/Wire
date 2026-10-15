
#include <basalt/wire/bedrock/BedrockBlocks.hpp>

#include <array>

namespace basalt::wire::bedrock {
namespace {

/// Plain frame first, then the glowing one, each down, up, north, south, west, east.
constexpr int kFrameStates = 12;

constexpr std::array<std::uint16_t, kFrameStates> kFrames2630{6477, 6478, 6479, 6480, 6481, 6482,
                                                              1047, 1048, 1049, 1050, 1051, 1052};
constexpr std::array<std::uint16_t, kFrameStates> kFrames2640{6907, 6908, 6909, 6910, 6911, 6912,
                                                              1059, 1060, 1061, 1062, 1063, 1064};
constexpr std::array<std::uint16_t, kFrameStates> kFrames2650{8563, 8564, 8565, 8566, 8567, 8568,
                                                              1208, 1209, 1210, 1211, 1212, 1213};

} // namespace

std::uint16_t itemFrameBlockId(const bool glowing, const int face, const int protocol) {
    if (face < 0 || face >= kFrameStates / 2) {
        return bedrockAirId(protocol);
    }
    const auto at = static_cast<std::size_t>(face) + (glowing ? kFrameStates / 2 : 0);
    if (protocol >= 2193) {
        return kFrames2650[at];
    }
    if (protocol >= 2168) {
        return kFrames2640[at];
    }
    return kFrames2630[at];
}

} // namespace basalt::wire::bedrock
