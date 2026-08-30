#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

namespace basalt::wire::protocol::play {
namespace {

constexpr std::uint8_t kOnGroundFlag = 1;

void writeComponent(io::ByteBuffer& buffer, const text::Component& component) {
    writeAnonymousNbt(buffer, text::componentNbt::serialize(component));
}

text::Component readComponent(io::ByteBuffer& buffer) {
    return text::componentNbt::deserialize(readAnonymousNbt(buffer));
}

template <typename Enum>
std::optional<Enum> fromOrdinal(std::int32_t ordinal, std::size_t count) noexcept {
    if (ordinal < 0 || static_cast<std::size_t>(ordinal) >= count) {
        return std::nullopt;
    }
    return static_cast<Enum>(ordinal);
}

} // namespace

void LoginPlayPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeInt(entityId);
    writeBoolean(buffer, isHardcore);
    varInt::write(buffer, static_cast<std::int32_t>(worldNames.size()));
    for (const std::string& name : worldNames) {
        writeString(buffer, name);
    }
    varInt::write(buffer, maxPlayers);
    varInt::write(buffer, viewDistance);
    varInt::write(buffer, simulationDistance);
    writeBoolean(buffer, reducedDebugInfo);
    writeBoolean(buffer, enableRespawnScreen);
    writeBoolean(buffer, doLimitedCrafting);
    spawnInfo.write(buffer);
    writeBoolean(buffer, onlineMode);
    writeBoolean(buffer, enforcesSecureChat);
}

std::optional<LoginPlayPacket> LoginPlayPacket::read(io::ByteBuffer& buffer) {
    LoginPlayPacket packet;
    packet.entityId = buffer.readInt();
    packet.isHardcore = readBoolean(buffer);

    const std::int32_t worldCount = readCollectionSize(buffer, 2);
    packet.worldNames.reserve(static_cast<std::size_t>(worldCount));
    for (std::int32_t i = 0; i < worldCount && !buffer.failed(); ++i) {
        packet.worldNames.push_back(readString(buffer));
    }

    packet.maxPlayers = varInt::read(buffer);
    packet.viewDistance = varInt::read(buffer);
    packet.simulationDistance = varInt::read(buffer);
    packet.reducedDebugInfo = readBoolean(buffer);
    packet.enableRespawnScreen = readBoolean(buffer);
    packet.doLimitedCrafting = readBoolean(buffer);

    auto spawnInfo = SpawnInfo::read(buffer);
    if (!spawnInfo) {
        return std::nullopt;
    }
    packet.spawnInfo = std::move(*spawnInfo);
    packet.onlineMode = readBoolean(buffer);
    packet.enforcesSecureChat = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void PlayerPositionPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, teleportId);
    buffer.writeDouble(x);
    buffer.writeDouble(y);
    buffer.writeDouble(z);
    buffer.writeDouble(dx);
    buffer.writeDouble(dy);
    buffer.writeDouble(dz);
    buffer.writeFloat(yaw);
    buffer.writeFloat(pitch);
    buffer.writeInt(relativeFlags);
}

std::optional<PlayerPositionPacket> PlayerPositionPacket::read(io::ByteBuffer& buffer) {
    PlayerPositionPacket packet;
    packet.teleportId = varInt::read(buffer);
    packet.x = buffer.readDouble();
    packet.y = buffer.readDouble();
    packet.z = buffer.readDouble();
    packet.dx = buffer.readDouble();
    packet.dy = buffer.readDouble();
    packet.dz = buffer.readDouble();
    packet.yaw = buffer.readFloat();
    packet.pitch = buffer.readFloat();
    packet.relativeFlags = buffer.readInt();
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void AcceptTeleportationPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, teleportId); }

std::optional<AcceptTeleportationPacket> AcceptTeleportationPacket::read(io::ByteBuffer& buffer) {
    AcceptTeleportationPacket packet{varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SpawnPositionPacket::write(io::ByteBuffer& buffer) const {
    globalPos.write(buffer);
    buffer.writeFloat(yaw);
    buffer.writeFloat(pitch);
}

std::optional<SpawnPositionPacket> SpawnPositionPacket::read(io::ByteBuffer& buffer) {
    SpawnPositionPacket packet;
    packet.globalPos = GlobalPos::read(buffer);
    packet.yaw = buffer.readFloat();
    packet.pitch = buffer.readFloat();
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

PlayerInfoUpdatePacket PlayerInfoUpdatePacket::initializing(std::vector<PlayerInfoEntry> entries) {
    return PlayerInfoUpdatePacket{kInitializing, std::move(entries)};
}

PlayerInfoUpdatePacket PlayerInfoUpdatePacket::gameMode(const common::Uuid& uuid, Gamemode gamemode) {
    PlayerInfoEntry entry;
    entry.uuid = uuid;
    entry.gamemode = gamemode;
    return PlayerInfoUpdatePacket{kUpdateGameMode, {std::move(entry)}};
}

void PlayerInfoUpdatePacket::write(io::ByteBuffer& buffer) const {
    buffer.writeByte(actions);
    varInt::write(buffer, static_cast<std::int32_t>(entries.size()));

    for (const PlayerInfoEntry& entry : entries) {
        writeUuid(buffer, entry.uuid);
        // The fields go out in the action enum's own order, which is the order of the bits.
        if ((actions & kAddPlayer) != 0) {
            writeString(buffer, entry.name);
            varInt::write(buffer, static_cast<std::int32_t>(entry.properties.size()));
            for (const login::GameProfileProperty& property : entry.properties) {
                writeString(buffer, property.name);
                writeString(buffer, property.value);
                writeBoolean(buffer, property.signature.has_value());
                if (property.signature) {
                    writeString(buffer, *property.signature);
                }
            }
        }
        if ((actions & kUpdateGameMode) != 0) {
            varInt::write(buffer, static_cast<std::int32_t>(entry.gamemode));
        }
        if ((actions & kUpdateListed) != 0) {
            writeBoolean(buffer, entry.listed);
        }
        if ((actions & kUpdateLatency) != 0) {
            varInt::write(buffer, entry.latencyMillis);
        }
    }
}

std::optional<PlayerInfoUpdatePacket> PlayerInfoUpdatePacket::read(io::ByteBuffer& buffer) {
    PlayerInfoUpdatePacket packet;
    packet.actions = buffer.readByte();
    if (buffer.failed()) {
        return std::nullopt;
    }
    // The four actions this build does not write carry fields of unknown width, so a packet with one
    // of them set cannot be walked past: refusing it is the only correct answer.
    constexpr std::uint8_t supported = kAddPlayer | kUpdateGameMode | kUpdateListed | kUpdateLatency;
    if ((packet.actions & static_cast<std::uint8_t>(~supported)) != 0) {
        buffer.markFailed();
        return std::nullopt;
    }

    const std::int32_t count = readCollectionSize(buffer, 16);
    packet.entries.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        PlayerInfoEntry entry;
        entry.uuid = readUuid(buffer);

        if ((packet.actions & kAddPlayer) != 0) {
            entry.name = readString(buffer, 16);
            const std::int32_t properties = readCollectionSize(buffer, 3);
            entry.properties.reserve(static_cast<std::size_t>(properties));
            for (std::int32_t k = 0; k < properties && !buffer.failed(); ++k) {
                login::GameProfileProperty property;
                property.name = readString(buffer);
                property.value = readString(buffer);
                if (readBoolean(buffer)) {
                    property.signature = readString(buffer);
                }
                entry.properties.push_back(std::move(property));
            }
        }
        if ((packet.actions & kUpdateGameMode) != 0) {
            const auto gamemode = fromOrdinal<Gamemode>(varInt::read(buffer), 4);
            if (!gamemode) {
                buffer.markFailed();
                return std::nullopt;
            }
            entry.gamemode = *gamemode;
        }
        if ((packet.actions & kUpdateListed) != 0) {
            entry.listed = readBoolean(buffer);
        }
        if ((packet.actions & kUpdateLatency) != 0) {
            entry.latencyMillis = varInt::read(buffer);
        }
        packet.entries.push_back(std::move(entry));
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void PlayerInfoRemovePacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(players.size()));
    for (const common::Uuid& uuid : players) {
        writeUuid(buffer, uuid);
    }
}

std::optional<PlayerInfoRemovePacket> PlayerInfoRemovePacket::read(io::ByteBuffer& buffer) {
    PlayerInfoRemovePacket packet;
    const std::int32_t count = readCollectionSize(buffer, 16);
    packet.players.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        packet.players.push_back(readUuid(buffer));
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void EntityTeleportPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    buffer.writeDouble(x);
    buffer.writeDouble(y);
    buffer.writeDouble(z);
    buffer.writeDouble(deltaX);
    buffer.writeDouble(deltaY);
    buffer.writeDouble(deltaZ);
    buffer.writeFloat(yaw);
    buffer.writeFloat(pitch);
    buffer.writeInt(relatives);
    writeBoolean(buffer, onGround);
}

std::optional<EntityTeleportPacket> EntityTeleportPacket::read(io::ByteBuffer& buffer) {
    EntityTeleportPacket packet;
    packet.entityId = varInt::read(buffer);
    packet.x = buffer.readDouble();
    packet.y = buffer.readDouble();
    packet.z = buffer.readDouble();
    packet.deltaX = buffer.readDouble();
    packet.deltaY = buffer.readDouble();
    packet.deltaZ = buffer.readDouble();
    packet.yaw = buffer.readFloat();
    packet.pitch = buffer.readFloat();
    packet.relatives = buffer.readInt();
    packet.onGround = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void PlayKeepAlivePacket::write(io::ByteBuffer& buffer) const { buffer.writeLong(keepAliveId); }

std::optional<PlayKeepAlivePacket> PlayKeepAlivePacket::read(io::ByteBuffer& buffer) {
    PlayKeepAlivePacket packet{buffer.readLong()};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void SystemChatPacket::write(io::ByteBuffer& buffer) const {
    writeComponent(buffer, content);
    writeBoolean(buffer, isActionBar);
}

std::optional<SystemChatPacket> SystemChatPacket::read(io::ByteBuffer& buffer) {
    SystemChatPacket packet;
    packet.content = readComponent(buffer);
    packet.isActionBar = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ChatMessagePacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, message);
    buffer.writeLong(timestamp);
    buffer.writeLong(salt);
    writeBoolean(buffer, signature.has_value());
    if (signature) {
        buffer.writeBytes(*signature);
    }
    varInt::write(buffer, offset);
    buffer.writeBytes(acknowledged);
    buffer.writeByte(static_cast<std::uint8_t>(checksum));
}

std::optional<ChatMessagePacket> ChatMessagePacket::read(io::ByteBuffer& buffer) {
    ChatMessagePacket packet;
    packet.message = readString(buffer, 256);
    packet.timestamp = buffer.readLong();
    packet.salt = buffer.readLong();
    if (readBoolean(buffer)) {
        packet.signature = buffer.readBytes(kSignatureLength);
    }
    packet.offset = varInt::read(buffer);
    packet.acknowledged = buffer.readBytes(kAcknowledgedLength);
    packet.checksum = static_cast<std::int8_t>(buffer.readByte());
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void PlayDisconnectPacket::write(io::ByteBuffer& buffer) const { writeComponent(buffer, reason); }

std::optional<PlayDisconnectPacket> PlayDisconnectPacket::read(io::ByteBuffer& buffer) {
    PlayDisconnectPacket packet{readComponent(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ServerboundPositionPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeDouble(x);
    buffer.writeDouble(y);
    buffer.writeDouble(z);
    buffer.writeByte(onGround ? kOnGroundFlag : 0);
}

std::optional<ServerboundPositionPacket> ServerboundPositionPacket::read(io::ByteBuffer& buffer) {
    ServerboundPositionPacket packet;
    packet.x = buffer.readDouble();
    packet.y = buffer.readDouble();
    packet.z = buffer.readDouble();
    packet.onGround = (buffer.readByte() & kOnGroundFlag) != 0;
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ServerboundPositionLookPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeDouble(x);
    buffer.writeDouble(y);
    buffer.writeDouble(z);
    buffer.writeFloat(yaw);
    buffer.writeFloat(pitch);
    buffer.writeByte(onGround ? kOnGroundFlag : 0);
}

std::optional<ServerboundPositionLookPacket> ServerboundPositionLookPacket::read(io::ByteBuffer& buffer) {
    ServerboundPositionLookPacket packet;
    packet.x = buffer.readDouble();
    packet.y = buffer.readDouble();
    packet.z = buffer.readDouble();
    packet.yaw = buffer.readFloat();
    packet.pitch = buffer.readFloat();
    packet.onGround = (buffer.readByte() & kOnGroundFlag) != 0;
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void BlockChangedAckPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, sequence); }

std::optional<BlockChangedAckPacket> BlockChangedAckPacket::read(io::ByteBuffer& buffer) {
    BlockChangedAckPacket packet{varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ServerboundPlayerActionPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(action));
    buffer.writeLong(pos.pack());
    buffer.writeByte(static_cast<std::uint8_t>(direction));
    varInt::write(buffer, sequence);
}

std::optional<ServerboundPlayerActionPacket> ServerboundPlayerActionPacket::read(io::ByteBuffer& buffer) {
    const auto action = fromOrdinal<Action>(varInt::read(buffer), 8);
    if (!action) {
        buffer.markFailed();
        return std::nullopt;
    }

    ServerboundPlayerActionPacket packet;
    packet.action = *action;
    packet.pos = BlockPos::unpack(buffer.readLong());
    const auto direction = fromOrdinal<Direction>(buffer.readByte(), 6);
    if (!direction) {
        buffer.markFailed();
        return std::nullopt;
    }
    packet.direction = *direction;
    packet.sequence = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void BlockHitResult::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(pos.pack());
    varInt::write(buffer, static_cast<std::int32_t>(direction));
    buffer.writeFloat(clickX);
    buffer.writeFloat(clickY);
    buffer.writeFloat(clickZ);
    writeBoolean(buffer, inside);
    writeBoolean(buffer, worldBorderHit);
}

std::optional<BlockHitResult> BlockHitResult::read(io::ByteBuffer& buffer) {
    BlockHitResult hit;
    hit.pos = BlockPos::unpack(buffer.readLong());
    const auto direction = fromOrdinal<Direction>(varInt::read(buffer), 6);
    if (!direction) {
        buffer.markFailed();
        return std::nullopt;
    }
    hit.direction = *direction;
    hit.clickX = buffer.readFloat();
    hit.clickY = buffer.readFloat();
    hit.clickZ = buffer.readFloat();
    hit.inside = readBoolean(buffer);
    hit.worldBorderHit = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{hit};
}

void ServerboundUseItemOnPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(hand));
    blockHit.write(buffer);
    varInt::write(buffer, sequence);
}

std::optional<ServerboundUseItemOnPacket> ServerboundUseItemOnPacket::read(io::ByteBuffer& buffer) {
    const auto hand = fromOrdinal<Hand>(varInt::read(buffer), 2);
    if (!hand) {
        buffer.markFailed();
        return std::nullopt;
    }
    auto hit = BlockHitResult::read(buffer);
    if (!hit) {
        return std::nullopt;
    }
    ServerboundUseItemOnPacket packet{*hand, *hit, varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ServerboundUseItemPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(hand));
    varInt::write(buffer, sequence);
    buffer.writeFloat(yaw);
    buffer.writeFloat(pitch);
}

std::optional<ServerboundUseItemPacket> ServerboundUseItemPacket::read(io::ByteBuffer& buffer) {
    const auto hand = fromOrdinal<Hand>(varInt::read(buffer), 2);
    if (!hand) {
        buffer.markFailed();
        return std::nullopt;
    }
    ServerboundUseItemPacket packet{*hand, varInt::read(buffer), buffer.readFloat(), buffer.readFloat()};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void PickItemFromEntityPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    writeBoolean(buffer, includeData);
}

std::optional<PickItemFromEntityPacket> PickItemFromEntityPacket::read(io::ByteBuffer& buffer) {
    PickItemFromEntityPacket packet{varInt::read(buffer), readBoolean(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void PickItemFromBlockPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeLong(pos.pack());
    writeBoolean(buffer, includeData);
}

std::optional<PickItemFromBlockPacket> PickItemFromBlockPacket::read(io::ByteBuffer& buffer) {
    PickItemFromBlockPacket packet;
    packet.pos = BlockPos::unpack(buffer.readLong());
    packet.includeData = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

} // namespace basalt::wire::protocol::play
