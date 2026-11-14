#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/multiversion/java/Registries764.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <algorithm>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
namespace configuration = protocol::configuration;
using protocol::ProtocolState;

/// parity: ClientboundFinishConfigurationPacket, numbered as this server does.
constexpr std::int32_t kFinishConfiguration = 3;
constexpr std::string_view kDimensionType = "minecraft:dimension_type";
/// The registry data packet as 1.20.2 to 1.20.4 number it.
constexpr std::int32_t kRegistryData764 = 0x05;

void writeSpawn(io::ByteBuffer& output, const LoginFields::SpawnFields& spawn, const std::string& dimensionType) {
    protocol::writeString(output, dimensionType);
    protocol::writeString(output, spawn.dimensionName);
    output.writeLong(spawn.hashedSeed);
    output.writeByte(static_cast<std::uint8_t>(spawn.gamemode));
    output.writeByte(static_cast<std::uint8_t>(spawn.previousGamemode));
    protocol::writeBoolean(output, spawn.debug);
    protocol::writeBoolean(output, spawn.flat);
    protocol::writeBoolean(output, spawn.deathDimension.has_value());
    if (spawn.deathDimension.has_value()) {
        protocol::writeString(output, *spawn.deathDimension);
        output.writeLong(spawn.deathPosition);
    }
    protocol::varInt::write(output, spawn.portalCooldown);
}

} // namespace

bool Registries764::clientbound(ProtocolState state, MutablePacket& packet) {
    if (!next_->clientbound(state, packet)) {
        return false;
    }
    if (state == ProtocolState::Configuration) {
        if (packet.nativeId() == configuration::RegistryDataPacket::kId) {
            remember(packet);
            return false;
        }
        if (packet.nativeId() == kFinishConfiguration) {
            sendCodec(packet);
        }
        return true;
    }
    if (state == ProtocolState::Play && packet.nativeId() == play::LoginPlayPacket::kId) {
        nameDimension(packet, true);
    } else if (state == ProtocolState::Play && packet.nativeId() == play::RespawnPacket::kId) {
        nameDimension(packet, false);
    }
    return true;
}

void Registries764::remember(const MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const auto data = configuration::RegistryDataPacket::read(input);
    if (!data.has_value()) {
        return;
    }
    std::vector<std::pair<std::string, nbt::NbtCompound>> entries;
    for (const configuration::RegistryEntry& entry : data->entries) {
        const nbt::NbtCompound* value = entry.value.has_value() ? entry.value->get<nbt::NbtCompound>() : nullptr;
        entries.emplace_back(entry.key, value == nullptr ? nbt::NbtCompound{} : *value);
    }
    std::erase_if(registries_, [&](const auto& registry) { return registry.first == data->registryId; });
    registries_.emplace_back(data->registryId, std::move(entries));
}

void Registries764::sendCodec(MutablePacket& packet) const {
    // The codec comes in the finish packet's place, and the finish right after it.
    packet.extras.emplace_back(packet.id, packet.body);
    nbt::NbtCompound codec;
    for (const auto& [registry, entries] : registries_) {
        nbt::NbtList values;
        for (std::size_t index = 0; index < entries.size(); ++index) {
            nbt::NbtCompound one;
            one.putString("name", entries[index].first);
            one.putInt("id", static_cast<std::int32_t>(index));
            one.set("element", nbt::NbtTag::ofCompound(entries[index].second));
            values.push(nbt::NbtTag::ofCompound(std::move(one)));
        }
        nbt::NbtCompound holder;
        holder.putString("type", registry);
        holder.set("value", nbt::NbtTag::ofList(std::move(values)));
        codec.set(registry, nbt::NbtTag::ofCompound(std::move(holder)));
    }
    io::ByteBuffer output;
    protocol::writeAnonymousNbt(output, nbt::NbtTag::ofCompound(std::move(codec)));
    packet.body = output.toVector();
    packet.id = kRegistryData764;
}

void Registries764::nameDimension(MutablePacket& packet, bool login) const {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    if (login) {
        const LoginFields fields = LoginFields::read(input, false, false);
        if (input.failed()) {
            return;
        }
        output.writeInt(fields.entityId);
        protocol::writeBoolean(output, fields.hardcore);
        protocol::varInt::write(output, static_cast<std::int32_t>(fields.worldNames.size()));
        for (const std::string& world : fields.worldNames) {
            protocol::writeString(output, world);
        }
        protocol::varInt::write(output, fields.maxPlayers);
        protocol::varInt::write(output, fields.viewDistance);
        protocol::varInt::write(output, fields.simulationDistance);
        protocol::writeBoolean(output, fields.reducedDebugInfo);
        protocol::writeBoolean(output, fields.respawnScreen);
        protocol::writeBoolean(output, fields.limitedCrafting);
        writeSpawn(output, fields.spawn, dimensionTypeName(fields.spawn.dimension));
    } else {
        const LoginFields::SpawnFields spawn = LoginFields::SpawnFields::read(input, false);
        const std::uint8_t kept = input.readByte();
        if (input.failed()) {
            return;
        }
        writeSpawn(output, spawn, dimensionTypeName(spawn.dimension));
        output.writeByte(kept);
    }
    packet.body = output.toVector();
}

std::string Registries764::dimensionTypeName(std::int32_t index) const {
    for (const auto& [registry, entries] : registries_) {
        if (registry == kDimensionType && index >= 0 && static_cast<std::size_t>(index) < entries.size()) {
            return entries[static_cast<std::size_t>(index)].first;
        }
    }
    return "minecraft:overworld";
}

} // namespace basalt::wire::multiversion::java
