#include <basalt/wire/bedrock/BedrockBlocks.hpp>

namespace basalt::wire::bedrock {

const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIdsFor(const int protocol) {
    // Each of these is the first protocol that reads that palette, in the order the window runs.
    // 26.45 is not in the list because it reads 26.40's: the palette did not change between them.
    if (protocol >= 2193) {
        return bedrockBlockIds2650();
    }
    if (protocol >= 2168) {
        return bedrockBlockIds2640();
    }
    return bedrockBlockIds2630();
}

} // namespace basalt::wire::bedrock
