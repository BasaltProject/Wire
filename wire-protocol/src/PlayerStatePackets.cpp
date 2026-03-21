#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <cstdint>

namespace basalt::wire::protocol::play {
namespace {

constexpr std::uint8_t kFlagInvulnerable = 1;
constexpr std::uint8_t kFlagFlying = 2;
constexpr std::uint8_t kFlagMayFly = 4;
constexpr std::uint8_t kFlagInstabuild = 8;

template <typename Enum>
std::optional<Enum> fromOrdinal(std::int32_t ordinal, std::int32_t count) noexcept {
    if (ordinal < 0 || ordinal >= count) {
        return std::nullopt;
    }
    return static_cast<Enum>(ordinal);
}

} // namespace

void GlobalPos::write(io::ByteBuffer& buffer) const {
    writeString(buffer, dimensionName);
    buffer.writeLong(location.pack());
}

GlobalPos GlobalPos::read(io::ByteBuffer& buffer) {
    GlobalPos pos;
    pos.dimensionName = readString(buffer);
    pos.location = BlockPos::unpack(buffer.readLong());
    return pos;
}

void SpawnInfo::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, dimension);
    writeString(buffer, dimensionName);
    buffer.writeLong(hashedSeed);
    varInt::write(buffer, static_cast<std::int32_t>(gamemode));
    // parity: GameType.OPTIONAL_STREAM_CODEC, zero for none and the id plus one otherwise.
    varInt::write(buffer, previousGamemode >= 0 && previousGamemode <= 3 ? previousGamemode + 1 : 0);
    writeBoolean(buffer, isDebug);
    writeBoolean(buffer, isFlat);
    writeBoolean(buffer, death.has_value());
    if (death) {
        death->write(buffer);
    }
    varInt::write(buffer, portalCooldown);
    varInt::write(buffer, seaLevel);
}

std::optional<SpawnInfo> SpawnInfo::read(io::ByteBuffer& buffer) {
    SpawnInfo info;
    info.dimension = varInt::read(buffer);
    info.dimensionName = readString(buffer);
    info.hashedSeed = buffer.readLong();
    const std::optional<Gamemode> gamemode = fromOrdinal<Gamemode>(varInt::read(buffer), 4);
    if (!gamemode) {
        return std::nullopt;
    }
    info.gamemode = *gamemode;
    info.previousGamemode = varInt::read(buffer) - 1;
    info.isDebug = readBoolean(buffer);
    info.isFlat = readBoolean(buffer);
    if (readBoolean(buffer)) {
        info.death = GlobalPos::read(buffer);
    }
    info.portalCooldown = varInt::read(buffer);
    info.seaLevel = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(info)};
}

void SetHealthPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeFloat(health);
    varInt::write(buffer, food);
    buffer.writeFloat(saturation);
}

std::optional<SetHealthPacket> SetHealthPacket::read(io::ByteBuffer& buffer) {
    SetHealthPacket packet{buffer.readFloat(), varInt::read(buffer), buffer.readFloat()};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SetExperiencePacket::write(io::ByteBuffer& buffer) const {
    buffer.writeFloat(progress);
    varInt::write(buffer, level);
    varInt::write(buffer, total);
}

std::optional<SetExperiencePacket> SetExperiencePacket::read(io::ByteBuffer& buffer) {
    SetExperiencePacket packet{buffer.readFloat(), varInt::read(buffer), varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ClientboundPlayerAbilitiesPacket::write(io::ByteBuffer& buffer) const {
    std::uint8_t flags = 0;
    if (invulnerable) {
        flags |= kFlagInvulnerable;
    }
    if (flying) {
        flags |= kFlagFlying;
    }
    if (mayFly) {
        flags |= kFlagMayFly;
    }
    if (instabuild) {
        flags |= kFlagInstabuild;
    }
    buffer.writeByte(flags);
    buffer.writeFloat(flyingSpeed);
    buffer.writeFloat(walkingSpeed);
}

std::optional<ClientboundPlayerAbilitiesPacket> ClientboundPlayerAbilitiesPacket::read(io::ByteBuffer& buffer) {
    const std::uint8_t flags = buffer.readByte();
    ClientboundPlayerAbilitiesPacket packet;
    packet.invulnerable = (flags & kFlagInvulnerable) != 0;
    packet.flying = (flags & kFlagFlying) != 0;
    packet.mayFly = (flags & kFlagMayFly) != 0;
    packet.instabuild = (flags & kFlagInstabuild) != 0;
    packet.flyingSpeed = buffer.readFloat();
    packet.walkingSpeed = buffer.readFloat();
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ServerboundPlayerAbilitiesPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeByte(flying ? kFlagFlying : 0);
}

std::optional<ServerboundPlayerAbilitiesPacket> ServerboundPlayerAbilitiesPacket::read(io::ByteBuffer& buffer) {
    ServerboundPlayerAbilitiesPacket packet{(buffer.readByte() & kFlagFlying) != 0};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void RespawnPacket::write(io::ByteBuffer& buffer) const {
    spawnInfo.write(buffer);
    buffer.writeByte(dataToKeep);
}

std::optional<RespawnPacket> RespawnPacket::read(io::ByteBuffer& buffer) {
    auto info = SpawnInfo::read(buffer);
    if (!info) {
        return std::nullopt;
    }
    RespawnPacket packet{std::move(*info), buffer.readByte()};
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ClientCommandPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(action));
}

std::optional<ClientCommandPacket> ClientCommandPacket::read(io::ByteBuffer& buffer) {
    const std::optional<Action> action = fromOrdinal<Action>(varInt::read(buffer), 3);
    if (buffer.failed() || !action) {
        return std::nullopt;
    }
    return ClientCommandPacket{*action};
}

void PlayerCommandPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    varInt::write(buffer, static_cast<std::int32_t>(action));
    varInt::write(buffer, data);
}

std::optional<PlayerCommandPacket> PlayerCommandPacket::read(io::ByteBuffer& buffer) {
    PlayerCommandPacket packet;
    packet.entityId = varInt::read(buffer);
    const std::optional<Action> action = fromOrdinal<Action>(varInt::read(buffer), 7);
    if (!action) {
        return std::nullopt;
    }
    packet.action = *action;
    packet.data = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void PlayerCombatKillPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, playerId);
    writeAnonymousNbt(buffer, text::componentNbt::serialize(message));
}

std::optional<PlayerCombatKillPacket> PlayerCombatKillPacket::read(io::ByteBuffer& buffer) {
    PlayerCombatKillPacket packet;
    packet.playerId = varInt::read(buffer);
    packet.message = text::componentNbt::deserialize(readAnonymousNbt(buffer));
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void RemoveEntitiesPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(entityIds.size()));
    for (const std::int32_t id : entityIds) {
        varInt::write(buffer, id);
    }
}

std::optional<RemoveEntitiesPacket> RemoveEntitiesPacket::read(io::ByteBuffer& buffer) {
    RemoveEntitiesPacket packet;
    const std::int32_t count = readCollectionSize(buffer);
    packet.entityIds.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        packet.entityIds.push_back(varInt::read(buffer));
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::play
