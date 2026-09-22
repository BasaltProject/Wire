#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

void LoginFields::SpawnFields::write(io::ByteBuffer& output, bool withSeaLevel) const {
    protocol::varInt::write(output, dimension);
    protocol::writeString(output, dimensionName);
    output.writeLong(hashedSeed);
    output.writeByte(static_cast<std::uint8_t>(gamemode));
    output.writeByte(static_cast<std::uint8_t>(previousGamemode));
    protocol::writeBoolean(output, debug);
    protocol::writeBoolean(output, flat);
    protocol::writeBoolean(output, deathDimension.has_value());
    if (deathDimension.has_value()) {
        protocol::writeString(output, *deathDimension);
        output.writeLong(deathPosition);
    }
    protocol::varInt::write(output, portalCooldown);
    if (withSeaLevel) {
        protocol::varInt::write(output, seaLevel);
    }
}

LoginFields::SpawnFields LoginFields::SpawnFields::read(io::ByteBuffer& input, bool withSeaLevel,
                                                        bool varIntGamemodes) {
    SpawnFields fields;
    fields.dimension = protocol::varInt::read(input);
    fields.dimensionName = protocol::readString(input);
    fields.hashedSeed = input.readLong();
    if (varIntGamemodes) {
        fields.gamemode = protocol::varInt::read(input);
        // parity: GameType.OPTIONAL_STREAM_CODEC, zero for none; -1 is how the byte said it.
        fields.previousGamemode = protocol::varInt::read(input) - 1;
    } else {
        fields.gamemode = input.readByte();
        fields.previousGamemode = input.readByte();
    }
    fields.debug = input.readByte() != 0;
    fields.flat = input.readByte() != 0;
    if (input.readByte() != 0) {
        fields.deathDimension = protocol::readString(input);
        fields.deathPosition = input.readLong();
    }
    fields.portalCooldown = protocol::varInt::read(input);
    if (withSeaLevel) {
        fields.seaLevel = protocol::varInt::read(input);
    }
    return fields;
}

void LoginFields::write(io::ByteBuffer& output, bool withOnlineMode, bool withSeaLevel) const {
    output.writeInt(entityId);
    protocol::writeBoolean(output, hardcore);
    protocol::varInt::write(output, static_cast<std::int32_t>(worldNames.size()));
    for (const std::string& name : worldNames) {
        protocol::writeString(output, name);
    }
    protocol::varInt::write(output, maxPlayers);
    protocol::varInt::write(output, viewDistance);
    protocol::varInt::write(output, simulationDistance);
    protocol::writeBoolean(output, reducedDebugInfo);
    protocol::writeBoolean(output, respawnScreen);
    protocol::writeBoolean(output, limitedCrafting);
    spawn.write(output, withSeaLevel);
    if (withOnlineMode) {
        protocol::writeBoolean(output, onlineMode);
    }
    protocol::writeBoolean(output, enforcesSecureChat);
}

LoginFields LoginFields::read(io::ByteBuffer& input, bool withOnlineMode, bool withSeaLevel, bool varIntGamemodes) {
    LoginFields fields;
    fields.entityId = input.readInt();
    fields.hardcore = input.readByte() != 0;
    const std::int32_t worlds = protocol::varInt::read(input);
    for (std::int32_t i = 0; i < worlds && !input.failed(); ++i) {
        fields.worldNames.push_back(protocol::readString(input));
    }
    fields.maxPlayers = protocol::varInt::read(input);
    fields.viewDistance = protocol::varInt::read(input);
    fields.simulationDistance = protocol::varInt::read(input);
    fields.reducedDebugInfo = input.readByte() != 0;
    fields.respawnScreen = input.readByte() != 0;
    fields.limitedCrafting = input.readByte() != 0;
    fields.spawn = SpawnFields::read(input, withSeaLevel, varIntGamemodes);
    if (withOnlineMode) {
        fields.onlineMode = input.readByte() != 0;
    }
    fields.enforcesSecureChat = input.readByte() != 0;
    return fields;
}

std::optional<protocol::play::BlockPos> LoginFields::deathBlock() const {
    if (!spawn.deathDimension.has_value()) {
        return std::nullopt;
    }
    return protocol::play::BlockPos::unpack(spawn.deathPosition);
}

} // namespace basalt::wire::multiversion::java
