#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/MapPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <utility>

namespace basalt::wire::protocol::play {

void MapItemDataPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, mapId);
    buffer.writeByte(static_cast<std::uint8_t>(scale));
    writeBoolean(buffer, locked);
    writeBoolean(buffer, decorations.has_value());
    if (decorations.has_value()) {
        varInt::write(buffer, static_cast<std::int32_t>(decorations->size()));
        for (const MapDecorationEntry& decoration : *decorations) {
            varInt::write(buffer, decoration.type);
            buffer.writeByte(static_cast<std::uint8_t>(decoration.x));
            buffer.writeByte(static_cast<std::uint8_t>(decoration.y));
            buffer.writeByte(static_cast<std::uint8_t>(decoration.rotation));
            writeBoolean(buffer, decoration.name.has_value());
            if (decoration.name.has_value()) {
                writeAnonymousNbt(buffer, text::componentNbt::serialize(*decoration.name));
            }
        }
    }
    // parity: a zero width stands for no patch at all.
    if (!patch.has_value() || patch->width == 0) {
        buffer.writeByte(0);
        return;
    }
    buffer.writeByte(patch->width);
    buffer.writeByte(patch->height);
    buffer.writeByte(patch->startX);
    buffer.writeByte(patch->startY);
    writeByteArray(buffer, patch->colors);
}

std::optional<MapItemDataPacket> MapItemDataPacket::read(io::ByteBuffer& buffer) {
    MapItemDataPacket packet;
    packet.mapId = varInt::read(buffer);
    packet.scale = static_cast<std::int8_t>(buffer.readByte());
    packet.locked = readBoolean(buffer);
    if (readBoolean(buffer)) {
        std::vector<MapDecorationEntry> decorations;
        const std::int32_t count = readCollectionSize(buffer, 5);
        for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
            MapDecorationEntry decoration;
            decoration.type = varInt::read(buffer);
            decoration.x = static_cast<std::int8_t>(buffer.readByte());
            decoration.y = static_cast<std::int8_t>(buffer.readByte());
            decoration.rotation = static_cast<std::int8_t>(buffer.readByte() & 15);
            if (readBoolean(buffer)) {
                decoration.name = text::componentNbt::deserialize(readAnonymousNbt(buffer));
            }
            decorations.push_back(std::move(decoration));
        }
        packet.decorations = std::move(decorations);
    }
    const std::uint8_t width = buffer.readByte();
    if (width > 0) {
        MapColorPatch patch;
        patch.width = width;
        patch.height = buffer.readByte();
        patch.startX = buffer.readByte();
        patch.startY = buffer.readByte();
        patch.colors = readByteArray(buffer);
        packet.patch = std::move(patch);
    }
    if (buffer.failed()) {
        return std::nullopt;
    }
    return packet;
}

} // namespace basalt::wire::protocol::play
