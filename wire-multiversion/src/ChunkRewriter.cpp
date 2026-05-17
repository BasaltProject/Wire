#include <basalt/wire/multiversion/ChunkRewriter.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/PalettedContainer.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::wire::multiversion {
namespace {

namespace varInt = protocol::varInt;
namespace paletted = protocol::play::palettedContainer;

} // namespace

std::vector<std::uint8_t> ChunkRewriter::rewrite(std::span<const std::uint8_t> sections) {
    io::ByteBuffer input = io::ByteBuffer::wrap(std::vector<std::uint8_t>{sections.begin(), sections.end()});
    io::ByteBuffer output;

    while (input.readableBytes() > 0 && !input.failed()) {
        const std::int16_t blockCount = input.readShort();
        output.writeShort(blockCount);
        const std::int16_t fluids = options_.readsFluidCount ? input.readShort() : (options_.fluidCount ? blockCount : 0);
        if (options_.fluidCount) {
            output.writeShort(fluids);
        }

        const std::vector<std::int32_t> blocks =
            paletted::read(input, kBlocksPerSection, paletted::kBlockMaxIndirectBits);
        if (input.failed() || blocks.size() != kBlocksPerSection) {
            return {};
        }

        for (std::size_t i = 0; i < kBlocksPerSection; ++i) {
            const std::int32_t mapped = blockStates_->map(blocks[i]);
            scratch_[i] = mapped == IdLookup::kNone ? options_.fallbackState : mapped;
        }
        writeContainer(output, scratch_, paletted::kBlockMinIndirectBits, paletted::kBlockMaxIndirectBits,
                       options_.olderDirectBits);
        copyContainer(input, output, kBiomesPerSection, paletted::kBiomeMaxIndirectBits);
    }
    return input.failed() ? std::vector<std::uint8_t>{} : output.toVector();
}

std::vector<std::uint8_t> ChunkRewriter::rewriteBiomes(std::span<const std::uint8_t> biomes) {
    io::ByteBuffer input = io::ByteBuffer::wrap(std::vector<std::uint8_t>{biomes.begin(), biomes.end()});
    io::ByteBuffer output;
    while (input.readableBytes() > 0 && !input.failed()) {
        copyContainer(input, output, kBiomesPerSection, paletted::kBiomeMaxIndirectBits);
    }
    return input.failed() ? std::vector<std::uint8_t>{} : output.toVector();
}

void ChunkRewriter::writeContainer(io::ByteBuffer& output, std::span<const std::int32_t> values, int minIndirectBits,
                                   int maxIndirectBits, int directBits) const {
    if (!options_.lengthPrefixedData) {
        paletted::write(output, values, minIndirectBits, maxIndirectBits, directBits);
        return;
    }

    io::ByteBuffer modern;
    paletted::write(modern, values, minIndirectBits, maxIndirectBits, directBits);

    const std::uint8_t bits = modern.readByte();
    output.writeByte(bits);
    if (bits == 0) {
        varInt::write(output, varInt::read(modern));
        // Leaving it out is a byte short and the client reads the next field as a length.
        varInt::write(output, 0);
        return;
    }
    if (bits <= maxIndirectBits) {
        const std::int32_t entries = varInt::read(modern);
        varInt::write(output, entries);
        for (std::int32_t i = 0; i < entries; ++i) {
            varInt::write(output, varInt::read(modern));
        }
    }

    const auto words = static_cast<std::int32_t>(modern.readableBytes() / 8);
    varInt::write(output, words);
    for (std::int32_t i = 0; i < words; ++i) {
        output.writeLong(modern.readLong());
    }
}

void ChunkRewriter::copyContainer(io::ByteBuffer& input, io::ByteBuffer& output, std::size_t size,
                                  int maxIndirectBits) const {
    const std::uint8_t bits = input.readByte();
    output.writeByte(bits);
    if (bits == 0) {
        varInt::write(output, varInt::read(input));
        if (options_.lengthPrefixedData) {
            varInt::write(output, 0);
        }
        return;
    }
    if (bits <= maxIndirectBits) {
        const std::int32_t entries = protocol::readCollectionSize(input);
        varInt::write(output, entries);
        for (std::int32_t i = 0; i < entries && !input.failed(); ++i) {
            varInt::write(output, varInt::read(input));
        }
    }

    const std::size_t valuesPerLong = 64 / bits;
    const auto words = static_cast<std::int32_t>((size + valuesPerLong - 1) / valuesPerLong);
    if (options_.lengthPrefixedData) {
        varInt::write(output, words);
    }
    for (std::int32_t i = 0; i < words && !input.failed(); ++i) {
        output.writeLong(input.readLong());
    }
}

} // namespace basalt::wire::multiversion
