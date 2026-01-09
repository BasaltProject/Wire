#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cstdint>

namespace basalt::wire::protocol::play {
namespace {

/// A bitset per world section, matching java.util.BitSet.toLongArray.
void writeLongBitset(io::ByteBuffer& buffer, const std::vector<std::int64_t>& bits) {
    varInt::write(buffer, static_cast<std::int32_t>(bits.size()));
    for (const std::int64_t word : bits) {
        buffer.writeLong(word);
    }
}

std::vector<std::int64_t> readLongBitset(io::ByteBuffer& buffer) {
    const std::int32_t count = readCollectionSize(buffer, 8);
    std::vector<std::int64_t> bits;
    bits.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        bits.push_back(buffer.readLong());
    }
    return bits;
}

void writeByteArrayList(io::ByteBuffer& buffer, const std::vector<std::vector<std::uint8_t>>& arrays) {
    varInt::write(buffer, static_cast<std::int32_t>(arrays.size()));
    for (const std::vector<std::uint8_t>& array : arrays) {
        writeByteArray(buffer, array);
    }
}

std::vector<std::vector<std::uint8_t>> readByteArrayList(io::ByteBuffer& buffer) {
    const std::int32_t count = readCollectionSize(buffer);
    std::vector<std::vector<std::uint8_t>> arrays;
    arrays.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        arrays.push_back(readByteArray(buffer));
    }
    return arrays;
}

} // namespace

std::string_view heightmapName(HeightmapType type) noexcept {
    switch (type) {
    case HeightmapType::WorldSurfaceWg:
        return "WORLD_SURFACE_WG";
    case HeightmapType::WorldSurface:
        return "WORLD_SURFACE";
    case HeightmapType::OceanFloorWg:
        return "OCEAN_FLOOR_WG";
    case HeightmapType::OceanFloor:
        return "OCEAN_FLOOR";
    case HeightmapType::MotionBlocking:
        return "MOTION_BLOCKING";
    case HeightmapType::MotionBlockingNoLeaves:
        return "MOTION_BLOCKING_NO_LEAVES";
    }
    return "MOTION_BLOCKING";
}

void Heightmap::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(type));
    writeLongBitset(buffer, data);
}

std::optional<Heightmap> Heightmap::read(io::ByteBuffer& buffer) {
    const std::int32_t type = varInt::read(buffer);
    if (buffer.failed() || type < 0 || type > static_cast<std::int32_t>(HeightmapType::MotionBlockingNoLeaves)) {
        buffer.markFailed();
        return std::nullopt;
    }
    Heightmap heightmap;
    heightmap.type = static_cast<HeightmapType>(type);
    heightmap.data = readLongBitset(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(heightmap)};
}

void GameEventPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeByte(event);
    buffer.writeFloat(param);
}

std::optional<GameEventPacket> GameEventPacket::read(io::ByteBuffer& buffer) {
    GameEventPacket packet{buffer.readByte(), buffer.readFloat()};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ChunkBlockEntityInfo::write(io::ByteBuffer& buffer) const {
    buffer.writeByte(packedXZ);
    buffer.writeShort(y);
    varInt::write(buffer, typeId);
    // An absent tag is written as TAG_End, which is the one byte vanilla sends for "nothing".
    writeAnonymousNbt(buffer, tag ? nbt::NbtTag::ofCompound(*tag) : nbt::NbtTag{});
}

std::optional<ChunkBlockEntityInfo> ChunkBlockEntityInfo::read(io::ByteBuffer& buffer) {
    ChunkBlockEntityInfo info;
    info.packedXZ = buffer.readByte();
    info.y = buffer.readShort();
    info.typeId = varInt::read(buffer);

    const nbt::NbtTag tag = readAnonymousNbt(buffer);
    if (const auto* compound = tag.get<nbt::NbtCompound>()) {
        info.tag = *compound;
    } else if (tag.type() != nbt::NbtType::End) {
        buffer.markFailed();
        return std::nullopt;
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(info)};
}

void LightData::write(io::ByteBuffer& buffer) const {
    writeLongBitset(buffer, skyLightMask);
    writeLongBitset(buffer, blockLightMask);
    writeLongBitset(buffer, emptySkyLightMask);
    writeLongBitset(buffer, emptyBlockLightMask);
    writeByteArrayList(buffer, skyLight);
    writeByteArrayList(buffer, blockLight);
}

std::optional<LightData> LightData::read(io::ByteBuffer& buffer) {
    LightData light;
    light.skyLightMask = readLongBitset(buffer);
    light.blockLightMask = readLongBitset(buffer);
    light.emptySkyLightMask = readLongBitset(buffer);
    light.emptyBlockLightMask = readLongBitset(buffer);
    light.skyLight = readByteArrayList(buffer);
    light.blockLight = readByteArrayList(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(light)};
}

void MapChunkPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeInt(x);
    buffer.writeInt(z);
    varInt::write(buffer, static_cast<std::int32_t>(heightmaps.size()));
    for (const Heightmap& heightmap : heightmaps) {
        heightmap.write(buffer);
    }
    writeByteArray(buffer, chunkData);
    varInt::write(buffer, static_cast<std::int32_t>(blockEntities.size()));
    for (const ChunkBlockEntityInfo& blockEntity : blockEntities) {
        blockEntity.write(buffer);
    }
    light.write(buffer);
}

std::optional<MapChunkPacket> MapChunkPacket::read(io::ByteBuffer& buffer) {
    MapChunkPacket packet;
    packet.x = buffer.readInt();
    packet.z = buffer.readInt();

    const std::int32_t heightmapCount = readCollectionSize(buffer, 2);
    packet.heightmaps.reserve(static_cast<std::size_t>(heightmapCount));
    for (std::int32_t i = 0; i < heightmapCount && !buffer.failed(); ++i) {
        auto heightmap = Heightmap::read(buffer);
        if (!heightmap) {
            return std::nullopt;
        }
        packet.heightmaps.push_back(std::move(*heightmap));
    }

    packet.chunkData = readByteArray(buffer);

    const std::int32_t blockEntityCount = readCollectionSize(buffer, 5);
    packet.blockEntities.reserve(static_cast<std::size_t>(blockEntityCount));
    for (std::int32_t i = 0; i < blockEntityCount && !buffer.failed(); ++i) {
        auto info = ChunkBlockEntityInfo::read(buffer);
        if (!info) {
            return std::nullopt;
        }
        packet.blockEntities.push_back(std::move(*info));
    }

    auto light = LightData::read(buffer);
    if (!light) {
        return std::nullopt;
    }
    packet.light = std::move(*light);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ForgetLevelChunkPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(static_cast<std::int64_t>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunkX))) |
                                               (static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunkZ)) << 32)));
}

std::optional<ForgetLevelChunkPacket> ForgetLevelChunkPacket::read(io::ByteBuffer& buffer) {
    const auto packed = static_cast<std::uint64_t>(buffer.readLong());
    ForgetLevelChunkPacket packet;
    packet.chunkX = static_cast<std::int32_t>(static_cast<std::uint32_t>(packed));
    packet.chunkZ = static_cast<std::int32_t>(static_cast<std::uint32_t>(packed >> 32));
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ChunkBatchFinishedPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, batchSize); }

std::optional<ChunkBatchFinishedPacket> ChunkBatchFinishedPacket::read(io::ByteBuffer& buffer) {
    ChunkBatchFinishedPacket packet{varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ChunkBatchReceivedPacket::write(io::ByteBuffer& buffer) const { buffer.writeFloat(desiredChunksPerTick); }

std::optional<ChunkBatchReceivedPacket> ChunkBatchReceivedPacket::read(io::ByteBuffer& buffer) {
    ChunkBatchReceivedPacket packet{buffer.readFloat()};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void UpdateLightPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, chunkX);
    varInt::write(buffer, chunkZ);
    light.write(buffer);
}

std::optional<UpdateLightPacket> UpdateLightPacket::read(io::ByteBuffer& buffer) {
    UpdateLightPacket packet;
    packet.chunkX = varInt::read(buffer);
    packet.chunkZ = varInt::read(buffer);
    auto light = LightData::read(buffer);
    if (!light) {
        return std::nullopt;
    }
    packet.light = std::move(*light);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void SetChunkCacheCenterPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, chunkX);
    varInt::write(buffer, chunkZ);
}

std::optional<SetChunkCacheCenterPacket> SetChunkCacheCenterPacket::read(io::ByteBuffer& buffer) {
    SetChunkCacheCenterPacket packet{varInt::read(buffer), varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SetChunkCacheRadiusPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, radius); }

std::optional<SetChunkCacheRadiusPacket> SetChunkCacheRadiusPacket::read(io::ByteBuffer& buffer) {
    SetChunkCacheRadiusPacket packet{varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SetSimulationDistancePacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, simulationDistance); }

std::optional<SetSimulationDistancePacket> SetSimulationDistancePacket::read(io::ByteBuffer& buffer) {
    SetSimulationDistancePacket packet{varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ClientboundBlockUpdatePacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(pos.pack());
    varInt::write(buffer, blockStateId);
}

std::optional<ClientboundBlockUpdatePacket> ClientboundBlockUpdatePacket::read(io::ByteBuffer& buffer) {
    ClientboundBlockUpdatePacket packet;
    packet.pos = BlockPos::unpack(buffer.readLong());
    packet.blockStateId = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SetTimePacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(gameTime);
    varInt::write(buffer, static_cast<std::int32_t>(clocks.size()));
    for (const ClockUpdate& clock : clocks) {
        // parity: WorldClock.STREAM_CODEC is holderRegistry, so this is the plain id. With one
        // added, `/time` in the overworld drove the End's clock and nothing changed.
        varInt::write(buffer, clock.clockId);
        varLong::write(buffer, clock.totalTicks);
        buffer.writeFloat(clock.partialTick);
        buffer.writeFloat(clock.rate);
    }
}

std::optional<SetTimePacket> SetTimePacket::read(io::ByteBuffer& buffer) {
    SetTimePacket packet;
    packet.gameTime = buffer.readLong();

    const std::int32_t count = readCollectionSize(buffer, 10);
    packet.clocks.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        ClockUpdate clock;
        clock.clockId = varInt::read(buffer);
        clock.totalTicks = varLong::read(buffer);
        clock.partialTick = buffer.readFloat();
        clock.rate = buffer.readFloat();
        packet.clocks.push_back(clock);
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ChangeDifficultyPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeByte(difficulty);
    writeBoolean(buffer, locked);
}

std::optional<ChangeDifficultyPacket> ChangeDifficultyPacket::read(io::ByteBuffer& buffer) {
    ChangeDifficultyPacket packet{buffer.readByte(), readBoolean(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SectionBlocksUpdatePacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(packSection(sectionX, sectionY, sectionZ));
    varInt::write(buffer, static_cast<std::int32_t>(changes.size()));
    for (const Change& change : changes) {
        varLong::write(buffer, (static_cast<std::int64_t>(change.blockStateId) << 12) |
                                   (static_cast<std::int64_t>(change.position) & 0xFFFLL));
    }
}

std::optional<SectionBlocksUpdatePacket> SectionBlocksUpdatePacket::read(io::ByteBuffer& buffer) {
    const auto packed = static_cast<std::uint64_t>(buffer.readLong());
    SectionBlocksUpdatePacket packet;
    // Shifting left then arithmetically right is how vanilla sign-extends each field.
    const auto sign = [](std::uint64_t value, int left, int right) {
        return static_cast<std::int32_t>(static_cast<std::int64_t>(value << left) >> right);
    };
    packet.sectionX = sign(packed, 0, 42);
    packet.sectionY = sign(packed, 44, 44);
    packet.sectionZ = sign(packed, 22, 42);

    const std::int32_t count = readCollectionSize(buffer);
    packet.changes.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        const auto change = static_cast<std::uint64_t>(varLong::read(buffer));
        packet.changes.push_back(
            Change{static_cast<std::int16_t>(change & 0xFFFULL), static_cast<std::int32_t>(change >> 12)});
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::play
