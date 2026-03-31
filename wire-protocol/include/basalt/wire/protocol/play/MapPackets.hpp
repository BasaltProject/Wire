#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::wire::protocol::play {

/// One marker on a map. parity: `MapDecoration.STREAM_CODEC`; the type is a `map_decoration_type` id.
struct MapDecorationEntry {
    std::int32_t type = 0;
    std::int8_t x = 0;
    std::int8_t y = 0;
    /// Sixteenths of a turn, 0 to 15.
    std::int8_t rotation = 0;
    std::optional<text::Component> name;

    friend bool operator==(const MapDecorationEntry&, const MapDecorationEntry&) = default;
};

/// A rectangle of colours, row by row. parity: `MapItemSavedData.MapPatch`.
struct MapColorPatch {
    std::uint8_t startX = 0;
    std::uint8_t startY = 0;
    std::uint8_t width = 0;
    std::uint8_t height = 0;
    std::vector<std::uint8_t> colors;

    friend bool operator==(const MapColorPatch&, const MapColorPatch&) = default;
};

/// Clientbound, id 0x34: a map's decorations, the dirty part of its colours, or both.
struct MapItemDataPacket {
    static constexpr std::int32_t kId = 0x34;

    std::int32_t mapId = 0;
    std::int8_t scale = 0;
    bool locked = false;
    std::optional<std::vector<MapDecorationEntry>> decorations;
    std::optional<MapColorPatch> patch;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<MapItemDataPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const MapItemDataPacket&, const MapItemDataPacket&) = default;
};

} // namespace basalt::wire::protocol::play
