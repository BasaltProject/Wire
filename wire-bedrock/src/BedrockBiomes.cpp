#include <basalt/wire/bedrock/BedrockBiomes.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace basalt::wire::bedrock {
namespace {

constexpr std::size_t kBlocks = 4096;

/// The strings the list refers to by index, each written once in the order first met.
class StringTable {
public:
    [[nodiscard]] std::uint16_t indexOf(const std::string_view value) {
        const auto found = std::ranges::find(strings_, value);
        if (found != strings_.end()) {
            return static_cast<std::uint16_t>(found - strings_.begin());
        }
        strings_.push_back(value);
        return static_cast<std::uint16_t>(strings_.size() - 1);
    }

    void write(io::ByteBuffer& buffer) const {
        writeVarUInt(buffer, static_cast<std::uint32_t>(strings_.size()));
        for (const std::string_view value : strings_) {
            writeString(buffer, value);
        }
    }

private:
    std::vector<std::string_view> strings_;
};

[[nodiscard]] std::uint8_t bitsFor(const std::size_t entries) {
    for (const std::uint8_t bits : std::array<std::uint8_t, 8>{1, 2, 3, 4, 5, 6, 8, 16}) {
        if (entries <= (std::size_t{1} << bits)) {
            return bits;
        }
    }
    return 16;
}

} // namespace

void writeBiomeDefinitionList(io::ByteBuffer& buffer) {
    StringTable strings;
    const std::span<const BedrockBiomeDefinition> definitions = bedrockBiomeDefinitions();
    const std::span<const std::string_view> tags = bedrockBiomeTags();
    writeVarUInt(buffer, static_cast<std::uint32_t>(definitions.size()));
    for (const BedrockBiomeDefinition& definition : definitions) {
        writeShortLE(buffer, strings.indexOf(definition.name));
        writeShortLE(buffer, static_cast<std::uint16_t>(-1));
        writeFloatLE(buffer, definition.temperature);
        writeFloatLE(buffer, definition.downfall);
        writeFloatLE(buffer, definition.foliageSnow);
        writeFloatLE(buffer, definition.depth);
        writeFloatLE(buffer, definition.scale);
        writeIntLE(buffer, static_cast<std::int32_t>(definition.mapWaterColour));
        buffer.writeByte(definition.rain ? 1 : 0);
        buffer.writeByte(definition.hasTags ? 1 : 0);
        if (definition.hasTags) {
            const std::span<const std::uint16_t> own = bedrockBiomeTagsOf(definition);
            writeVarUInt(buffer, static_cast<std::uint32_t>(own.size()));
            for (const std::uint16_t tag : own) {
                writeShortLE(buffer, strings.indexOf(tags[tag]));
            }
        }
        buffer.writeByte(0);
    }
    strings.write(buffer);
}

void writeBiomeSection(io::ByteBuffer& buffer, const BiomeQuarts& quarts) {
    std::vector<std::int32_t> palette;
    std::array<std::uint16_t, 64> indices{};
    for (std::size_t quart = 0; quart < quarts.size(); ++quart) {
        const auto found = std::ranges::find(palette, quarts[quart]);
        indices[quart] = static_cast<std::uint16_t>(found - palette.begin());
        if (found == palette.end()) {
            palette.push_back(quarts[quart]);
        }
    }
    if (palette.size() == 1) {
        buffer.writeByte(0x01);
        writeVarInt(buffer, palette.front());
        return;
    }
    const std::uint8_t bits = bitsFor(palette.size());
    buffer.writeByte(static_cast<std::uint8_t>((static_cast<unsigned>(bits) << 1U) | 1U));
    const std::size_t perWord = 32U / static_cast<std::size_t>(bits);
    for (std::size_t at = 0; at < kBlocks; at += perWord) {
        std::uint32_t word = 0;
        for (std::size_t slot = 0; slot < perWord && at + slot < kBlocks; ++slot) {
            const std::size_t block = at + slot;
            const std::size_t x = (block >> 10U) & 3U;
            const std::size_t z = (block >> 6U) & 3U;
            const std::size_t y = (block >> 2U) & 3U;
            word |= static_cast<std::uint32_t>(indices[(((y << 2U) | z) << 2U) | x]) << (slot * bits);
        }
        writeIntLE(buffer, static_cast<std::int32_t>(word));
    }
    writeVarInt(buffer, static_cast<std::int32_t>(palette.size()));
    for (const std::int32_t biome : palette) {
        writeVarInt(buffer, biome);
    }
}

} // namespace basalt::wire::bedrock
