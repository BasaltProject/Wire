#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::bedrock {

/// Where a map update dropped its flag word and put a presence flag in front of each part instead,
/// and wrote its colours as little-endian ints rather than varints.
inline constexpr int kMapOptionalPartsProtocol = 2168;

/// One marker. The image is Bedrock's own icon number and the colour is ARGB.
struct BedrockMapDecoration {
    std::uint8_t image = 0;
    std::uint8_t rotation = 0;
    std::int8_t x = 0;
    std::int8_t y = 0;
    std::string label;
    std::uint32_t color = 0xFFFFFFFFU;

    friend bool operator==(const BedrockMapDecoration&, const BedrockMapDecoration&) = default;
};

/// A rectangle of pixels, row by row, each ABGR.
struct BedrockMapTexture {
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::vector<std::uint32_t> colors;

    friend bool operator==(const BedrockMapTexture&, const BedrockMapTexture&) = default;
};

/// `ClientboundMapItemData`. A client draws a marker only beside a tracked object, so every
/// decoration travels with a made-up entity of its own index.
struct BedrockMapData {
    std::int64_t mapId = 0;
    std::uint8_t dimension = 0;
    bool locked = false;
    std::int8_t scale = 0;
    /// The maps this one is made from, which a client wants to hear before it draws anything.
    std::vector<std::int64_t> created;
    std::optional<std::vector<BedrockMapDecoration>> decorations;
    std::optional<BedrockMapTexture> texture;

    friend bool operator==(const BedrockMapData&, const BedrockMapData&) = default;
};

void writeMapItemData(io::ByteBuffer& buffer, const BedrockMapData& map, int protocol);

[[nodiscard]] std::optional<BedrockMapData> readMapItemData(io::ByteBuffer& buffer, int protocol);

/// `MapInfoRequest`, a client asking for a map it holds and has never been sent: its id.
[[nodiscard]] std::optional<std::int64_t> readMapInfoRequest(io::ByteBuffer& buffer);

} // namespace basalt::wire::bedrock
