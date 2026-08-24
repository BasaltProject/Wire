#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/multiversion/java/Link768Up.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/VehiclePackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

void wrapBiomeMusic(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    auto data = protocol::configuration::RegistryDataPacket::read(in);
    if (!data.has_value() || data->registryId != "minecraft:worldgen/biome") {
        return;
    }
    for (protocol::configuration::RegistryEntry& entry : data->entries) {
        nbt::NbtCompound* element = entry.value ? entry.value->get<nbt::NbtCompound>() : nullptr;
        nbt::NbtTag* effects = element != nullptr ? element->find("effects") : nullptr;
        nbt::NbtCompound* effectsTable = effects != nullptr ? effects->get<nbt::NbtCompound>() : nullptr;
        nbt::NbtTag* music = effectsTable != nullptr ? effectsTable->find("music") : nullptr;
        if (music == nullptr || music->get<nbt::NbtCompound>() == nullptr) {
            continue;
        }
        nbt::NbtCompound weighted;
        weighted.set("data", std::move(*music));
        weighted.putInt("weight", 1);
        nbt::NbtList list;
        static_cast<void>(list.push(nbt::NbtTag::ofCompound(std::move(weighted))));
        *music = nbt::NbtTag::ofList(std::move(list));
    }
    io::ByteBuffer out;
    data->write(out);
    packet.body = out.toVector();
}

} // namespace

bool Link768Up::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    static const std::int32_t kRegistry =
        javaTranslators::packetIdAt(protocolVersions::kV1_21_2, protocol::ProtocolState::Configuration, true,
                                    protocol::configuration::RegistryDataPacket::kId);
    static const std::int32_t kHeldSlot = javaTranslators::packetIdAt(
        protocolVersions::kV1_21_2, protocol::ProtocolState::Play, true, play::SetHeldSlotPacket::kId);
    static const std::int32_t kParticles = javaTranslators::packetIdAt(
        protocolVersions::kV1_21_2, protocol::ProtocolState::Play, true, play::LevelParticlesPacket::kId);
    if (state == protocol::ProtocolState::Configuration && packet.id == kRegistry) {
        wrapBiomeMusic(packet);
    } else if (state == protocol::ProtocolState::Play && packet.id == kHeldSlot) {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t slot = in.readByte();
        if (!in.failed()) {
            io::ByteBuffer out;
            protocol::varInt::write(out, slot);
            packet.body = out.toVector();
        }
    } else if (state == protocol::ProtocolState::Play && packet.id == kParticles && !packet.body.empty()) {
        io::ByteBuffer out;
        out.writeByte(packet.body.front());
        out.writeByte(0);
        out.writeBytes(std::span<const std::uint8_t>{packet.body}.subspan(1));
        packet.body = out.toVector();
    }
    return RenumberingLink::clientbound(state, packet);
}

bool Link768Up::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state == protocol::ProtocolState::Play && packet.nativeId() == play::ServerboundMoveVehiclePacket::kId &&
        !packet.body.empty()) {
        packet.body.pop_back();
    }
    return RenumberingLink::serverbound(state, packet);
}

} // namespace basalt::wire::multiversion::java
