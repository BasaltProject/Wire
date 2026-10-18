#include <basalt/wire/multiversion/java/Link776Up.hpp>
#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/multiversion/java/LongMaskLight.hpp>
#include <basalt/wire/multiversion/java/NewRegistries776.hpp>
#include <basalt/wire/multiversion/java/Tables776.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/AdvancementPackets.hpp>
#include <basalt/wire/protocol/play/CommandPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
namespace configuration = protocol::configuration;
using protocol::ProtocolState;
using Shape = play::ParticleOptionShape;

/// parity: the three move packets of 26.3, which have no struct here because the server never sends them.
constexpr std::int32_t kMoveEntityPos = 54;
constexpr std::int32_t kMoveEntityPosRot = 55;
constexpr std::int32_t kMoveEntityRot = 57;

constexpr std::int32_t kOlderDialogParser = 55;
constexpr std::int32_t kOlderUuidParser = 56;

constexpr std::int32_t kStepTicks = 3;
constexpr std::int32_t kSteppedFlag = 1 << 1;
constexpr std::int32_t kChangeDestroyDirection = 1;
constexpr std::int16_t kBrewTimeSlot = 2;
constexpr std::int16_t kFuelSlot = 3;
constexpr std::int16_t kDefaultBrewTime = 400;
constexpr std::int16_t kDefaultFuel = 20;

constexpr std::array<std::pair<std::int32_t, Shape>, 22> kParticleShapes = {{
    {1, Shape::BlockState},      {2, Shape::BlockState},   {7, Shape::Geyser},  {8, Shape::GeyserBase},
    {9, Shape::GeyserBase},      {10, Shape::Geyser},      {15, Shape::Number}, {21, Shape::Dust},
    {22, Shape::DustTransition}, {23, Shape::Spell},       {28, Shape::Color},  {36, Shape::BlockState},
    {46, Shape::Color},          {48, Shape::Number},      {52, Shape::Color},  {56, Shape::Spell},
    {57, Shape::Item},           {58, Shape::Vibration},   {59, Shape::Trail},  {115, Shape::Delay},
    {121, Shape::BlockState},    {125, Shape::BlockState},
}};

Shape shapeOf(std::int32_t type) {
    const auto found = std::ranges::find(kParticleShapes, type, &std::pair<std::int32_t, Shape>::first);
    return found == kParticleShapes.end() ? Shape::None : found->second;
}

std::string_view trimPrefix() { return "trim/"; }

void upgradeTrimMaterial(nbt::NbtCompound& material) {
    const std::string asset{material.getString("asset_name")};
    material.remove("asset_name");
    material.remove("override_armor_assets");
    material.putString("palette_id", "minecraft:" + std::string{trimPrefix()} + asset);
}

} // namespace

struct Link776Up::Ids {
    std::int32_t loginPlay;
    std::int32_t respawn;
    std::int32_t animate;
    std::int32_t entityPositionSync;
    std::int32_t particles;
    std::int32_t explode;
    std::int32_t openSignEditor;
    std::int32_t mapChunk;
    std::int32_t updateLight;
    std::int32_t openScreen;
    std::int32_t containerSetData;
    std::int32_t commands;
    std::int32_t updateAdvancements;
    std::int32_t movePos;
    std::int32_t movePosRot;
    std::int32_t moveRot;
    std::int32_t registryData;
    std::int32_t tags;
    std::int32_t finishConfiguration;
    std::int32_t acceptTeleport;
    std::int32_t swing;
    std::int32_t playerAction;
    std::int32_t positionLook;
    std::int32_t signUpdate;
    std::int32_t brewingMenu;

    Ids() {
        const PacketIdTables& packets = tables776().packets;
        const auto clientbound = [&](std::int32_t id) { return packets.toOlder(ProtocolState::Play, id); };
        const auto serverbound = [&](std::int32_t id) { return packets.serverboundToOlder(ProtocolState::Play, id); };
        const auto config = [&](std::int32_t id) { return packets.toOlder(ProtocolState::Configuration, id); };
        loginPlay = clientbound(play::LoginPlayPacket::kId);
        respawn = clientbound(play::RespawnPacket::kId);
        animate = clientbound(play::AnimatePacket::kId);
        entityPositionSync = clientbound(play::EntityPositionSyncPacket::kId);
        particles = clientbound(play::LevelParticlesPacket::kId);
        explode = clientbound(play::ExplodePacket::kId);
        openSignEditor = clientbound(play::OpenSignEditorPacket::kId);
        mapChunk = clientbound(play::MapChunkPacket::kId);
        updateLight = clientbound(play::UpdateLightPacket::kId);
        openScreen = clientbound(play::OpenScreenPacket::kId);
        containerSetData = clientbound(play::ContainerSetDataPacket::kId);
        commands = clientbound(play::CommandsPacket::kId);
        updateAdvancements = clientbound(play::UpdateAdvancementsPacket::kId);
        movePos = clientbound(kMoveEntityPos);
        movePosRot = clientbound(kMoveEntityPosRot);
        moveRot = clientbound(kMoveEntityRot);
        registryData = config(configuration::RegistryDataPacket::kId);
        tags = config(configuration::TagsPacket::kId);
        finishConfiguration = config(configuration::kFinishConfigurationId);
        acceptTeleport = serverbound(play::AcceptTeleportationPacket::kId);
        swing = serverbound(play::kPunchId);
        playerAction = serverbound(play::ServerboundPlayerActionPacket::kId);
        positionLook = serverbound(play::ServerboundPositionLookPacket::kId);
        signUpdate = serverbound(play::SignUpdatePacket::kId);
        brewingMenu = tables776().menu.map(play::menuType::kBrewingStand);
    }
};

Link776Up::Link776Up(const VersionTables& upward) : RenumberingLink(upward) {
    static const Ids ids;
    ids_ = &ids;
}

bool Link776Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    if (state == ProtocolState::Configuration) {
        return configuration(packet);
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    const std::int32_t id = packet.id;
    if (id == ids_->loginPlay) {
        LoginFields login = decode(packet, [](io::ByteBuffer& in) { return LoginFields::read(in, true, true, false); });
        login.spawn.previousGamemode = static_cast<std::int8_t>(login.spawn.previousGamemode);
        encode(packet, [&](io::ByteBuffer& out) { login.write(out, true, true, true); });
    } else if (id == ids_->respawn) {
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            LoginFields::SpawnFields spawn = LoginFields::SpawnFields::read(input, true, false);
            spawn.previousGamemode = static_cast<std::int8_t>(spawn.previousGamemode);
            const std::uint8_t dataToKeep = input.readByte();
            spawn.write(output, true, true);
            output.writeByte(dataToKeep);
        });
    } else if (id == ids_->animate) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = protocol::varInt::read(input);
        const std::uint8_t action = input.readByte();
        if (action == 0 || action == 3) {
            packet.overrideId(play::SwingAnimationPacket::kId);
            encode(packet, [&](io::ByteBuffer& out) {
                play::SwingAnimationPacket{.entityId = entityId, .hand = action == 0 ? 0 : 1, .animation = {}}.write(out);
            });
        } else if (action == 2 || action == 4 || action == 5) {
            encode(packet, [&](io::ByteBuffer& out) {
                protocol::varInt::write(out, entityId);
                out.writeByte(action == 2 ? 0 : (action == 4 ? 1 : 2));
            });
        } else {
            return false;
        }
    } else if (id == ids_->entityPositionSync) {
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            protocol::varInt::write(output, protocol::varInt::read(input));
            protocol::varInt::write(output, 1);
            protocol::varInt::write(output, 1);
            output.writeDouble(input.readDouble());
            output.writeDouble(input.readDouble());
            output.writeDouble(input.readDouble());
            protocol::varInt::write(output, kStepTicks);
            static_cast<void>(input.readSpan(3 * 8));
            output.writeBytes(input.readable());
        });
    } else if (id == ids_->movePos) {
        moveEntity(packet, 0);
    } else if (id == ids_->movePosRot) {
        moveEntity(packet, 1);
    } else if (id == ids_->moveRot) {
        moveEntity(packet, 2);
    } else if (id == ids_->particles) {
        particles(packet);
    } else if (id == ids_->explode) {
        packet.body.push_back(1);
    } else if (id == ids_->openSignEditor) {
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            output.writeLong(input.readLong());
            protocol::varInt::write(output, input.readByte() != 0 ? 1 : 0);
        });
    } else if (id == ids_->mapChunk) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        auto chunk = play::MapChunkPacket::read(input, false);
        const auto light = readLongMaskLight(input);
        if (chunk.has_value() && light.has_value()) {
            chunk->light = *light;
            encode(packet, [&](io::ByteBuffer& out) { chunk->write(out); });
        }
    } else if (id == ids_->updateLight) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t chunkX = protocol::varInt::read(input);
        const std::int32_t chunkZ = protocol::varInt::read(input);
        const auto light = readLongMaskLight(input);
        if (light.has_value()) {
            encode(packet, [&](io::ByteBuffer& out) {
                play::UpdateLightPacket{.chunkX = chunkX, .chunkZ = chunkZ, .light = *light}.write(out);
            });
        }
    } else if (id == ids_->openScreen) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t containerId = protocol::varInt::read(input);
        if (protocol::varInt::read(input) == ids_->brewingMenu) {
            const auto data = [&](std::int16_t slot, std::int16_t value) {
                io::ByteBuffer out;
                protocol::varInt::write(out, containerId);
                out.writeShort(slot);
                out.writeShort(value);
                packet.extras.emplace_back(ids_->containerSetData, out.toVector());
            };
            data(kBrewTimeSlot, kDefaultBrewTime);
            data(kFuelSlot, kDefaultFuel);
        }
    } else if (id == ids_->commands) {
        auto commands = decode(packet, [](io::ByteBuffer& in) { return play::CommandsPacket::read(in); });
        if (!commands.has_value()) {
            return true;
        }
        for (play::CommandNodeData& node : commands->nodes) {
            if (!node.parser.has_value()) {
                continue;
            }
            if (node.parser->registryId() == kOlderDialogParser) {
                node.parser = play::ArgumentParser{play::argumentType::kDialog, node.parser->properties()};
            } else if (node.parser->registryId() == kOlderUuidParser) {
                node.parser = play::ArgumentParser{play::argumentType::kUuid, node.parser->properties()};
            }
        }
        encode(packet, [&](io::ByteBuffer& out) { commands->write(out); });
    } else if (id == ids_->updateAdvancements) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const auto advancements = play::UpdateAdvancementsPacket::read(input, true);
        if (advancements.has_value()) {
            encode(packet, [&](io::ByteBuffer& out) { advancements->write(out); });
        }
    }
    return true;
}

bool Link776Up::configuration(MutablePacket& packet) const {
    const std::int32_t id = packet.id;
    if (id == ids_->registryData) {
        auto data = decode(packet, [](io::ByteBuffer& in) { return configuration::RegistryDataPacket::read(in); });
        if (!data.has_value() || data->registryId != "minecraft:trim_material") {
            return true;
        }
        for (configuration::RegistryEntry& entry : data->entries) {
            if (entry.value.has_value()) {
                if (nbt::NbtCompound* material = entry.value->get<nbt::NbtCompound>()) {
                    upgradeTrimMaterial(*material);
                }
            }
        }
        encode(packet, [&](io::ByteBuffer& out) { data->write(out); });
    } else if (id == ids_->tags) {
        auto tags = decode(packet, [](io::ByteBuffer& in) { return configuration::TagsPacket::read(in); });
        if (!tags.has_value()) {
            return true;
        }
        for (configuration::TagGroup& group : tags->groups) {
            if (group.registryId != "minecraft:block") {
                continue;
            }
            for (configuration::TagEntry& tag : group.tags) {
                if (tag.name == "minecraft:convertable_to_mud") {
                    tag.name = "minecraft:convertible_to_mud";
                }
            }
        }
        encode(packet, [&](io::ByteBuffer& out) { tags->write(out); });
    } else if (id == ids_->finishConfiguration) {
        const auto added = registriesAddedIn26_3();
        if (added.empty()) {
            return true;
        }
        const std::vector<std::uint8_t> finish = packet.body;
        packet.overrideId(configuration::RegistryDataPacket::kId);
        packet.body.assign(added[0].begin(), added[0].end());
        for (std::size_t index = 1; index < added.size(); ++index) {
            packet.extras.emplace_back(ids_->registryData, std::vector<std::uint8_t>{added[index].begin(), added[index].end()});
        }
        packet.extras.emplace_back(ids_->finishConfiguration, finish);
    }
    return true;
}

void Link776Up::moveEntity(MutablePacket& packet, std::int32_t kind) const {
    rewrite(packet, [kind](io::ByteBuffer& input, io::ByteBuffer& output) {
        protocol::varInt::write(output, protocol::varInt::read(input));
        if (kind == 2) {
            const std::uint8_t yRot = input.readByte();
            const std::uint8_t xRot = input.readByte();
            const std::uint8_t onGround = input.readByte();
            output.writeByte(onGround);
            output.writeByte(yRot);
            output.writeByte(xRot);
            return;
        }
        const std::int16_t xa = input.readShort();
        const std::int16_t ya = input.readShort();
        const std::int16_t za = input.readShort();
        std::uint8_t yRot = 0;
        std::uint8_t xRot = 0;
        if (kind == 1) {
            yRot = input.readByte();
            xRot = input.readByte();
        }
        const bool onGround = input.readByte() != 0;
        protocol::varInt::write(output, (onGround ? 1 : 0) | kSteppedFlag);
        protocol::varInt::write(output, kStepTicks);
        output.writeShort(xa);
        output.writeShort(ya);
        output.writeShort(za);
        if (kind == 1) {
            output.writeByte(yRot);
            output.writeByte(xRot);
        }
    });
}

void Link776Up::particles(MutablePacket& packet) const {
    rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
        const auto bools = input.readSpan(2);
        const auto position = input.readSpan(3 * 8 + 3 * 4);
        const float speed = input.readFloat();
        const std::int32_t count = input.readInt();
        const std::int32_t type = protocol::varInt::read(input);
        const Shape shape = shapeOf(tables776().particleType.unmap(type));
        std::optional<play::ParticleOptions> options;
        if (shape != Shape::None) {
            options = play::ParticleOptions::read(input, shape);
            if (!options.has_value()) {
                return;
            }
        }
        protocol::varInt::write(output, type);
        if (options.has_value()) {
            options->write(output);
        }
        output.writeBytes(bools);
        output.writeBytes(position);
        output.writeFloat(speed);
        output.writeFloat(speed);
        output.writeFloat(speed);
        protocol::varInt::write(output, count);
        protocol::varInt::write(output, 0);
        if (input.failed()) {
            output.clear();
        }
    });
}

bool Link776Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::serverbound(state, packet)) {
        return false;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    const std::int32_t id = packet.nativeId();
    if (id == ids_->acceptTeleport) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t teleportId = protocol::varInt::read(input);
        const auto position = input.readSpan(3 * 8 + 2 * 4);
        if (input.failed()) {
            return false;
        }
        io::ByteBuffer move;
        move.writeBytes(position);
        move.writeByte(0);
        packet.extras.emplace_back(ids_->positionLook, move.toVector());
        encode(packet, [&](io::ByteBuffer& out) { protocol::varInt::write(out, teleportId); });
    } else if (id == ids_->swing) {
        if (packet.body.empty()) {
            packet.body.push_back(0);
        }
    } else if (id == ids_->playerAction) {
        io::ByteBuffer peek = io::ByteBuffer::wrap(packet.body);
        if (protocol::varInt::read(peek) == kChangeDestroyDirection) {
            return false;
        }
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            const std::int32_t action = protocol::varInt::read(input);
            protocol::varInt::write(output, action > kChangeDestroyDirection ? action - 1 : action);
            output.writeBytes(input.readable());
        });
    } else if (id == ids_->signUpdate) {
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            output.writeLong(input.readLong());
            std::array<std::string, play::SignUpdatePacket::kLines> lines;
            for (std::string& line : lines) {
                line = protocol::readString(input, play::SignUpdatePacket::kMaxLineLength);
            }
            const bool front = protocol::varInt::read(input) == 1;
            output.writeByte(front ? 1 : 0);
            for (const std::string& line : lines) {
                protocol::writeString(output, line);
            }
        });
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
