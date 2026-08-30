#include <basalt/wire/multiversion/java/Link763.hpp>
#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <algorithm>
#include <array>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

constexpr std::string_view kDimensionType = "minecraft:dimension_type";
constexpr std::string_view kChatType = "minecraft:chat_type";

/// Everything a client of 1.20.1 looks for in the codec, in the order vanilla writes it.
constexpr std::array<std::string_view, 3> kCodecRegistries = {kDimensionType, "minecraft:worldgen/biome", kChatType};

[[nodiscard]] std::pair<std::string, nbt::NbtCompound> defaultChatType() {
    nbt::NbtCompound decoration;
    decoration.putString("translation_key", "chat.type.text");
    nbt::NbtList parameters;
    parameters.push(nbt::NbtTag::ofString("sender"));
    parameters.push(nbt::NbtTag::ofString("content"));
    decoration.set("parameters", nbt::NbtTag::ofList(std::move(parameters)));
    decoration.set("style", nbt::NbtTag::ofCompound(nbt::NbtCompound{}));
    nbt::NbtCompound element;
    element.set("chat", nbt::NbtTag::ofCompound(decoration));
    element.set("narration", nbt::NbtTag::ofCompound(std::move(decoration)));
    return {std::string{"minecraft:chat"}, std::move(element)};
}

} // namespace

bool Link763::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Configuration) {
        if (packet.nativeId() == protocol::configuration::RegistryDataPacket::kId) {
            rememberRegistry(packet);
        }
        // Nothing from the configuration reaches a client that has no such phase.
        return false;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    if (packet.nativeId() == play::RespawnPacket::kId) {
        // The spawn arrives without its sea level (the link to 1.21 took it out) and has to come out
        // named rather than numbered, like the one inside the login packet.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const LoginFields::SpawnFields spawn = LoginFields::SpawnFields::read(input, false);
        const std::uint8_t keep = input.readByte();
        if (input.failed()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::writeString(output, dimensionTypeName(spawn.dimension));
            protocol::writeString(output, spawn.dimensionName);
            output.writeLong(spawn.hashedSeed);
            output.writeByte(static_cast<std::uint8_t>(spawn.gamemode));
            output.writeByte(static_cast<std::uint8_t>(spawn.previousGamemode));
            protocol::writeBoolean(output, spawn.debug);
            protocol::writeBoolean(output, spawn.flat);
            output.writeByte(keep);
            protocol::writeBoolean(output, spawn.deathDimension.has_value());
            if (spawn.deathDimension.has_value()) {
                protocol::writeString(output, *spawn.deathDimension);
                output.writeLong(spawn.deathPosition);
            }
            protocol::varInt::write(output, spawn.portalCooldown);
        });
        return true;
    }
    if (packet.nativeId() != play::LoginPlayPacket::kId) {
        return true;
    }
    // By here the two links above have taken the online-mode flag and the sea level out of it.
    const LoginFields login = decode(packet, [](io::ByteBuffer& in) { return LoginFields::read(in, false, false); });
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(login.entityId);
        protocol::writeBoolean(output, login.hardcore);
        output.writeByte(static_cast<std::uint8_t>(login.spawn.gamemode));
        output.writeByte(static_cast<std::uint8_t>(login.spawn.previousGamemode));
        protocol::varInt::write(output, static_cast<std::int32_t>(login.worldNames.size()));
        for (const std::string& name : login.worldNames) {
            protocol::writeString(output, name);
        }
        protocol::writeAnonymousNbt(output, nbt::NbtTag::ofCompound(codec()));
        protocol::writeString(output, dimensionTypeName(login.spawn.dimension));
        protocol::writeString(output, login.spawn.dimensionName);
        output.writeLong(login.spawn.hashedSeed);
        protocol::varInt::write(output, login.maxPlayers);
        protocol::varInt::write(output, login.viewDistance);
        protocol::varInt::write(output, login.simulationDistance);
        protocol::writeBoolean(output, login.reducedDebugInfo);
        protocol::writeBoolean(output, login.respawnScreen);
        protocol::writeBoolean(output, login.spawn.debug);
        protocol::writeBoolean(output, login.spawn.flat);
        protocol::writeBoolean(output, login.spawn.deathDimension.has_value());
        if (login.spawn.deathDimension.has_value()) {
            protocol::writeString(output, *login.spawn.deathDimension);
            output.writeLong(login.spawn.deathPosition);
        }
        protocol::varInt::write(output, login.spawn.portalCooldown);
    });
    return true;
}

void Link763::rememberRegistry(MutablePacket& packet) {
    const auto data =
        decode(packet, [](io::ByteBuffer& in) { return protocol::configuration::RegistryDataPacket::read(in); });
    if (!data.has_value() || std::ranges::find(kCodecRegistries, data->registryId) == kCodecRegistries.end()) {
        return;
    }
    std::vector<std::pair<std::string, nbt::NbtCompound>> entries;
    entries.reserve(data->entries.size());
    for (const protocol::configuration::RegistryEntry& entry : data->entries) {
        if (!entry.value.has_value()) {
            continue;
        }
        if (const nbt::NbtCompound* value = entry.value->get<nbt::NbtCompound>()) {
            entries.emplace_back(entry.key, *value);
        }
    }
    registries_[data->registryId] = std::move(entries);
}

nbt::NbtCompound Link763::codec() const {
    nbt::NbtCompound codec;
    for (const std::string_view registry : kCodecRegistries) {
        std::vector<std::pair<std::string, nbt::NbtCompound>> entries;
        const auto found = registries_.find(std::string{registry});
        if (found != registries_.end()) {
            entries = found->second;
        } else if (registry == kChatType) {
            entries.push_back(defaultChatType());
        } else {
            continue;
        }
        nbt::NbtList values;
        for (std::size_t index = 0; index < entries.size(); ++index) {
            nbt::NbtCompound one;
            one.putString("name", entries[index].first);
            one.putInt("id", static_cast<std::int32_t>(index));
            one.set("element", nbt::NbtTag::ofCompound(entries[index].second));
            values.push(nbt::NbtTag::ofCompound(std::move(one)));
        }
        nbt::NbtCompound holder;
        holder.putString("type", std::string{registry});
        holder.set("value", nbt::NbtTag::ofList(std::move(values)));
        codec.set(registry, nbt::NbtTag::ofCompound(std::move(holder)));
    }
    return codec;
}

std::string Link763::dimensionTypeName(std::int32_t index) const {
    const auto found = registries_.find(std::string{kDimensionType});
    if (found == registries_.end() || index < 0 || static_cast<std::size_t>(index) >= found->second.size()) {
        return "minecraft:overworld";
    }
    return found->second[static_cast<std::size_t>(index)].first;
}

} // namespace basalt::wire::multiversion::java
