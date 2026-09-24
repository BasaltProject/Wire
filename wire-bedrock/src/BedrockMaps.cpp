#include <basalt/wire/bedrock/BedrockMaps.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>

#include <utility>

namespace basalt::wire::bedrock {
namespace {

constexpr std::uint32_t kTextureFlag = 0x02;
constexpr std::uint32_t kDecorationFlag = 0x04;
constexpr std::uint32_t kCreationFlag = 0x08;
/// The size a client could possibly ask for, which caps what a read allocates.
constexpr std::uint32_t kMaxPixels = 128 * 128;
constexpr std::uint32_t kMaxDecorations = 1024;

void writeBool(io::ByteBuffer& buffer, bool value) { buffer.writeByte(value ? 1 : 0); }

[[nodiscard]] bool readBool(io::ByteBuffer& buffer) { return buffer.readByte() != 0; }

void writeColor(io::ByteBuffer& buffer, std::uint32_t color, bool newer) {
    if (newer) {
        writeIntLE(buffer, static_cast<std::int32_t>(color));
    } else {
        writeVarUInt(buffer, color);
    }
}

[[nodiscard]] std::uint32_t readColor(io::ByteBuffer& buffer, bool newer) {
    return newer ? static_cast<std::uint32_t>(readIntLE(buffer)) : readVarUInt(buffer);
}

void writeDecoration(io::ByteBuffer& buffer, const BedrockMapDecoration& decoration, bool newer) {
    buffer.writeByte(decoration.image);
    buffer.writeByte(decoration.rotation);
    buffer.writeByte(static_cast<std::uint8_t>(decoration.x));
    buffer.writeByte(static_cast<std::uint8_t>(decoration.y));
    writeString(buffer, decoration.label);
    writeColor(buffer, decoration.color, newer);
}

[[nodiscard]] BedrockMapDecoration readDecoration(io::ByteBuffer& buffer, bool newer) {
    BedrockMapDecoration decoration;
    decoration.image = buffer.readByte();
    decoration.rotation = buffer.readByte();
    decoration.x = static_cast<std::int8_t>(buffer.readByte());
    decoration.y = static_cast<std::int8_t>(buffer.readByte());
    decoration.label = readString(buffer);
    decoration.color = readColor(buffer, newer);
    return decoration;
}

void writeTexture(io::ByteBuffer& buffer, const BedrockMapTexture& texture, bool newer) {
    if (newer) {
        for (const std::int32_t value : {texture.width, texture.height, texture.x, texture.y}) {
            writeBool(buffer, true);
            writeVarInt(buffer, value);
        }
        writeBool(buffer, true);
    } else {
        for (const std::int32_t value : {texture.width, texture.height, texture.x, texture.y}) {
            writeVarInt(buffer, value);
        }
    }
    writeVarUInt(buffer, static_cast<std::uint32_t>(texture.colors.size()));
    for (const std::uint32_t color : texture.colors) {
        writeColor(buffer, color, newer);
    }
}

void writeNewer(io::ByteBuffer& buffer, const BedrockMapData& map) {
    writeVarLong(buffer, map.mapId);
    buffer.writeByte(map.dimension);
    writeBool(buffer, map.locked);
    // The origin, which only a map drawn by the client itself fills in.
    writeVarInt(buffer, 0);
    writeVarInt(buffer, 0);
    writeVarInt(buffer, 0);
    writeBool(buffer, !map.created.empty());
    if (!map.created.empty()) {
        writeVarUInt(buffer, static_cast<std::uint32_t>(map.created.size()));
        for (const std::int64_t id : map.created) {
            writeVarLong(buffer, id);
        }
    }
    writeBool(buffer, true);
    buffer.writeByte(static_cast<std::uint8_t>(map.scale));
    const bool decorated = map.decorations.has_value();
    writeBool(buffer, decorated);
    if (decorated) {
        writeVarUInt(buffer, static_cast<std::uint32_t>(map.decorations->size()));
        for (std::size_t index = 0; index < map.decorations->size(); ++index) {
            writeIntLE(buffer, 0); // An entity,
            writeBool(buffer, true);
            writeVarLong(buffer, static_cast<std::int64_t>(index));
            writeBool(buffer, false); // not a block.
        }
    }
    writeBool(buffer, decorated);
    if (decorated) {
        writeVarUInt(buffer, static_cast<std::uint32_t>(map.decorations->size()));
        for (const BedrockMapDecoration& decoration : *map.decorations) {
            writeDecoration(buffer, decoration, true);
        }
    }
    if (map.texture.has_value()) {
        writeTexture(buffer, *map.texture, true);
    } else {
        for (int part = 0; part < 5; ++part) {
            writeBool(buffer, false);
        }
    }
}

void writeOlder(io::ByteBuffer& buffer, const BedrockMapData& map) {
    writeVarLong(buffer, map.mapId);
    std::uint32_t flags = 0;
    if (map.texture.has_value() && !map.texture->colors.empty()) {
        flags |= kTextureFlag;
    }
    if (map.decorations.has_value() && !map.decorations->empty()) {
        flags |= kDecorationFlag;
    }
    if (!map.created.empty()) {
        flags |= kCreationFlag;
    }
    writeVarUInt(buffer, flags);
    buffer.writeByte(map.dimension);
    writeBool(buffer, map.locked);
    writeVarInt(buffer, 0);
    writeVarInt(buffer, 0);
    writeVarInt(buffer, 0);
    if ((flags & kCreationFlag) != 0) {
        writeVarUInt(buffer, static_cast<std::uint32_t>(map.created.size()));
        for (const std::int64_t id : map.created) {
            writeVarLong(buffer, id);
        }
    }
    if (flags != 0) {
        buffer.writeByte(static_cast<std::uint8_t>(map.scale));
    }
    if ((flags & kDecorationFlag) != 0) {
        writeVarUInt(buffer, static_cast<std::uint32_t>(map.decorations->size()));
        for (std::size_t index = 0; index < map.decorations->size(); ++index) {
            writeIntLE(buffer, 0);
            writeVarLong(buffer, static_cast<std::int64_t>(index));
        }
        writeVarUInt(buffer, static_cast<std::uint32_t>(map.decorations->size()));
        for (const BedrockMapDecoration& decoration : *map.decorations) {
            writeDecoration(buffer, decoration, false);
        }
    }
    if ((flags & kTextureFlag) != 0) {
        writeTexture(buffer, *map.texture, false);
    }
}

[[nodiscard]] std::optional<BedrockMapData> readNewer(io::ByteBuffer& buffer) {
    BedrockMapData map;
    map.mapId = readVarLong(buffer);
    map.dimension = buffer.readByte();
    map.locked = readBool(buffer);
    static_cast<void>(readVarInt(buffer));
    static_cast<void>(readVarInt(buffer));
    static_cast<void>(readVarInt(buffer));
    if (readBool(buffer)) {
        const std::uint32_t count = readVarUInt(buffer);
        for (std::uint32_t i = 0; i < count && i < kMaxDecorations && !buffer.failed(); ++i) {
            map.created.push_back(readVarLong(buffer));
        }
    }
    if (readBool(buffer)) {
        map.scale = static_cast<std::int8_t>(buffer.readByte());
    }
    if (readBool(buffer)) {
        const std::uint32_t count = readVarUInt(buffer);
        for (std::uint32_t i = 0; i < count && i < kMaxDecorations && !buffer.failed(); ++i) {
            static_cast<void>(readIntLE(buffer));
            if (readBool(buffer)) {
                static_cast<void>(readVarLong(buffer));
            }
            if (readBool(buffer)) {
                static_cast<void>(readVarInt(buffer));
                static_cast<void>(readVarUInt(buffer));
                static_cast<void>(readVarInt(buffer));
            }
        }
    }
    if (readBool(buffer)) {
        std::vector<BedrockMapDecoration> decorations;
        const std::uint32_t count = readVarUInt(buffer);
        for (std::uint32_t i = 0; i < count && i < kMaxDecorations && !buffer.failed(); ++i) {
            decorations.push_back(readDecoration(buffer, true));
        }
        map.decorations = std::move(decorations);
    }
    BedrockMapTexture texture;
    bool textured = false;
    for (std::int32_t* field : {&texture.width, &texture.height, &texture.x, &texture.y}) {
        if (readBool(buffer)) {
            *field = readVarInt(buffer);
            textured = true;
        }
    }
    if (readBool(buffer)) {
        const std::uint32_t count = readVarUInt(buffer);
        for (std::uint32_t i = 0; i < count && i < kMaxPixels && !buffer.failed(); ++i) {
            texture.colors.push_back(readColor(buffer, true));
        }
        textured = true;
    }
    if (textured) {
        map.texture = std::move(texture);
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(map)};
}

[[nodiscard]] std::optional<BedrockMapData> readOlder(io::ByteBuffer& buffer) {
    BedrockMapData map;
    map.mapId = readVarLong(buffer);
    const std::uint32_t flags = readVarUInt(buffer);
    map.dimension = buffer.readByte();
    map.locked = readBool(buffer);
    static_cast<void>(readVarInt(buffer));
    static_cast<void>(readVarInt(buffer));
    static_cast<void>(readVarInt(buffer));
    if ((flags & kCreationFlag) != 0) {
        const std::uint32_t count = readVarUInt(buffer);
        for (std::uint32_t i = 0; i < count && i < kMaxDecorations && !buffer.failed(); ++i) {
            map.created.push_back(readVarLong(buffer));
        }
    }
    if (flags != 0) {
        map.scale = static_cast<std::int8_t>(buffer.readByte());
    }
    if ((flags & kDecorationFlag) != 0) {
        const std::uint32_t objects = readVarUInt(buffer);
        for (std::uint32_t i = 0; i < objects && i < kMaxDecorations && !buffer.failed(); ++i) {
            if (readIntLE(buffer) == 0) {
                static_cast<void>(readVarLong(buffer));
            } else {
                static_cast<void>(readVarInt(buffer));
                static_cast<void>(readVarUInt(buffer));
                static_cast<void>(readVarInt(buffer));
            }
        }
        std::vector<BedrockMapDecoration> decorations;
        const std::uint32_t count = readVarUInt(buffer);
        for (std::uint32_t i = 0; i < count && i < kMaxDecorations && !buffer.failed(); ++i) {
            decorations.push_back(readDecoration(buffer, false));
        }
        map.decorations = std::move(decorations);
    }
    if ((flags & kTextureFlag) != 0) {
        BedrockMapTexture texture;
        texture.width = readVarInt(buffer);
        texture.height = readVarInt(buffer);
        texture.x = readVarInt(buffer);
        texture.y = readVarInt(buffer);
        const std::uint32_t count = readVarUInt(buffer);
        for (std::uint32_t i = 0; i < count && i < kMaxPixels && !buffer.failed(); ++i) {
            texture.colors.push_back(readColor(buffer, false));
        }
        map.texture = std::move(texture);
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(map)};
}

} // namespace

void writeMapItemData(io::ByteBuffer& buffer, const BedrockMapData& map, const int protocol) {
    if (protocol >= kMapOptionalPartsProtocol) {
        writeNewer(buffer, map);
    } else {
        writeOlder(buffer, map);
    }
}

std::optional<BedrockMapData> readMapItemData(io::ByteBuffer& buffer, const int protocol) {
    return protocol >= kMapOptionalPartsProtocol ? readNewer(buffer) : readOlder(buffer);
}

std::optional<std::int64_t> readMapInfoRequest(io::ByteBuffer& buffer) {
    const std::int64_t id = readVarLong(buffer);
    return buffer.failed() ? std::nullopt : std::optional{id};
}

} // namespace basalt::wire::bedrock
