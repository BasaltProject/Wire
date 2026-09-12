#include <basalt/wire/bedrock/BedrockBlocks.hpp>
#include <basalt/wire/bedrock/BedrockPacketIds.hpp>

namespace basalt::wire::bedrock {

const std::array<std::uint16_t, kJavaBlockStates>& bedrockBlockIdsFor(const int protocol) {
    // Each of these is the first protocol that reads that palette, in the order the window runs.
    // 26.45 is not in the list because it reads 26.40's: the palette did not change between them.
    if (protocol >= 2168) {
        return bedrockBlockIds2640();
    }
    if (protocol >= 1001) {
        return bedrockBlockIds2630();
    }
    if (protocol >= 975) {
        return bedrockBlockIds2620();
    }
    if (protocol >= 944) {
        return bedrockBlockIds2610();
    }
    return bedrockBlockIds260();
}

} // namespace basalt::wire::bedrock
