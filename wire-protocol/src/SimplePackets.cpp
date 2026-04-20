#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <cstdint>

namespace basalt::wire::protocol::play {

void CooldownPacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, cooldownGroup);
    varInt::write(buffer, durationTicks);
}

std::optional<CooldownPacket> CooldownPacket::read(io::ByteBuffer& buffer) {
    CooldownPacket packet;
    packet.cooldownGroup = readString(buffer);
    packet.durationTicks = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void CustomPayloadPacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, channel);
    buffer.writeBytes(data);
}

std::optional<CustomPayloadPacket> CustomPayloadPacket::read(io::ByteBuffer& buffer) {
    CustomPayloadPacket packet;
    packet.channel = readString(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    packet.data = buffer.readBytes(buffer.readableBytes());
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void AwardStatsPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(stats.size()));
    for (const StatCounter& stat : stats) {
        varInt::write(buffer, stat.statType);
        varInt::write(buffer, stat.value);
        varInt::write(buffer, stat.count);
    }
}

std::optional<AwardStatsPacket> AwardStatsPacket::read(io::ByteBuffer& buffer) {
    AwardStatsPacket packet;
    const std::int32_t count = readCollectionSize(buffer, 3);
    packet.stats.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        packet.stats.push_back(StatCounter{varInt::read(buffer), varInt::read(buffer), varInt::read(buffer)});
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ChunkBiomeData::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(packChunkPos(chunkX, chunkZ));
    writeByteArray(buffer, biomes);
}

ChunkBiomeData ChunkBiomeData::read(io::ByteBuffer& buffer) {
    const auto packed = static_cast<std::uint64_t>(buffer.readLong());
    ChunkBiomeData data;
    data.chunkX = static_cast<std::int32_t>(static_cast<std::uint32_t>(packed));
    data.chunkZ = static_cast<std::int32_t>(static_cast<std::uint32_t>(packed >> 32));
    data.biomes = readByteArray(buffer);
    return data;
}

void ChunksBiomesPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(chunks.size()));
    for (const ChunkBiomeData& chunk : chunks) {
        chunk.write(buffer);
    }
}

std::optional<ChunksBiomesPacket> ChunksBiomesPacket::read(io::ByteBuffer& buffer) {
    ChunksBiomesPacket packet;
    const std::int32_t count = readCollectionSize(buffer, 9);
    packet.chunks.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        packet.chunks.push_back(ChunkBiomeData::read(buffer));
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ExplodePacket::write(io::ByteBuffer& buffer) const {
    buffer.writeDouble(x);
    buffer.writeDouble(y);
    buffer.writeDouble(z);
    buffer.writeFloat(radius);
    // parity: a plain int here, not a varint, because the codec is ByteBufCodecs.INT.
    buffer.writeInt(blockCount);

    writeBoolean(buffer, knockbackX.has_value());
    if (knockbackX) {
        buffer.writeDouble(*knockbackX);
        buffer.writeDouble(knockbackY);
        buffer.writeDouble(knockbackZ);
    }

    varInt::write(buffer, particleId);
    varInt::write(buffer, 0);
    writeString(buffer, sound);
    writeBoolean(buffer, false);

    varInt::write(buffer, static_cast<std::int32_t>(blockParticles.size()));
    for (const ExplosionParticle& particle : blockParticles) {
        varInt::write(buffer, particle.particleId);
        buffer.writeFloat(particle.scaling);
        buffer.writeFloat(particle.speed);
        varInt::write(buffer, particle.weight);
    }
    writeBoolean(buffer, playSound);
}

std::optional<ExplodePacket> ExplodePacket::read(io::ByteBuffer& buffer) {
    ExplodePacket packet;
    packet.x = buffer.readDouble();
    packet.y = buffer.readDouble();
    packet.z = buffer.readDouble();
    packet.radius = buffer.readFloat();
    packet.blockCount = buffer.readInt();

    if (readBoolean(buffer)) {
        packet.knockbackX = buffer.readDouble();
        packet.knockbackY = buffer.readDouble();
        packet.knockbackZ = buffer.readDouble();
    }

    packet.particleId = varInt::read(buffer);
    static_cast<void>(varInt::read(buffer));
    packet.sound = readString(buffer);
    static_cast<void>(readBoolean(buffer));

    const std::int32_t count = readCollectionSize(buffer, 10);
    packet.blockParticles.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        packet.blockParticles.push_back(
            ExplosionParticle{varInt::read(buffer), buffer.readFloat(), buffer.readFloat(), varInt::read(buffer)});
    }
    packet.playSound = buffer.readableBytes() == 0 || readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::play
