#include <basalt/wire/bedrock/BedrockColumn.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <array>

namespace basalt::wire::bedrock {

std::vector<std::uint8_t> encodeColumn(const ColumnShape& shape, const FillBlocks& blocks, const FillBiomes& biomes,
                                       std::span<const std::uint8_t> blockEntities) {
    io::ByteBuffer packed{static_cast<std::size_t>(shape.sent) * 1024};
    std::array<std::int32_t, kSubChunkBlocks> section{};
    for (std::int32_t index = 0; index < shape.sent; ++index) {
        blocks(index, section);
        writeSubChunk(packed, section, static_cast<std::int8_t>(shape.minSection + index), shape.protocol);
    }

    io::ByteBuffer biomeBytes{static_cast<std::size_t>(shape.sections) * 4};
    BiomeQuarts quarts{};
    for (std::int32_t index = 0; index < shape.sections; ++index) {
        biomes(index, quarts);
        writeBiomeSection(biomeBytes, quarts);
    }

    io::ByteBuffer body;
    writeLevelChunk(body, shape.chunkX, shape.chunkZ, static_cast<std::uint32_t>(shape.sent), packed.readable(),
                    static_cast<std::uint32_t>(shape.sections), shape.dimension, shape.protocol, blockEntities,
                    biomeBytes.readable());
    return body.toVector();
}

} // namespace basalt::wire::bedrock
