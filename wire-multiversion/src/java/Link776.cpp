#include <basalt/wire/multiversion/java/Link776.hpp>
#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/multiversion/java/LongMaskLight.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/CommandPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

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

constexpr std::int32_t kAnimate776 = 0x02;
constexpr std::uint8_t kSwingMainHand776 = 0;
constexpr std::uint8_t kSwingOffHand776 = 3;

/// parity: ServerboundPlayerActionPacket.Action.CHANGE_DESTROY_DIRECTION.
constexpr std::int32_t kChangeDestroyDirection = 1;

/// The brewing stand's total brew time and total fuel, the slots 26.3 added after the two it had.
constexpr std::int16_t kFirstNewBrewingSlot = 2;

/// The particle types that carry options, by this server's numbering.
constexpr std::array<std::pair<std::int32_t, Shape>, 22> kParticleShapes = {{
    {1, Shape::BlockState},      {2, Shape::BlockState},   {7, Shape::Geyser},  {8, Shape::GeyserBase},
    {9, Shape::GeyserBase},      {10, Shape::Geyser},      {15, Shape::Number}, {21, Shape::Dust},
    {22, Shape::DustTransition}, {23, Shape::Spell},       {28, Shape::Color},  {36, Shape::BlockState},
    {46, Shape::Color},          {48, Shape::Number},      {52, Shape::Color},  {56, Shape::Spell},
    {57, Shape::Item},           {58, Shape::Vibration},   {59, Shape::Trail},  {115, Shape::Delay},
    {121, Shape::BlockState},    {125, Shape::BlockState},
}};

constexpr std::array<std::string_view, 3> kNewRegistries = {
    "minecraft:decorated_pot_pattern", "minecraft:block_transformer", "minecraft:worldgen/block_state_provider"};

constexpr std::array<std::string_view, 2> kNewPropertySets = {"minecraft:brewing_input", "minecraft:brewing_reagent"};

/// parity: the 26.2 trim materials that darkened on armour of their own metal.
constexpr std::array<std::string_view, 5> kDarkerOnOwnArmour = {"copper", "diamond", "gold", "iron", "netherite"};

constexpr std::string_view kTrimPalettePrefix = "trim/";

play::ArgumentParser olderArgument(const play::ArgumentParser& parser) {
    namespace type = play::argumentType;
    switch (parser.registryId()) {
    case type::kDialog:
        return play::ArgumentParser{55, parser.properties()};
    case type::kUuid:
        return play::ArgumentParser{56, parser.properties()};
    case type::kContextFloatProvider:
    case type::kContextIntProvider:
    case type::kSlotSource:
    case type::kFeature:
    case type::kSwingAnimation:
        return play::ArgumentParser{type::kBrigadierString, play::ArgumentParser::StringProperties{
                                                                play::ArgumentParser::StringKind::QuotablePhrase}};
    default:
        return parser;
    }
}

Shape shapeOf(std::int32_t nativeType) {
    const auto found = std::ranges::find(kParticleShapes, nativeType, &std::pair<std::int32_t, Shape>::first);
    return found == kParticleShapes.end() ? Shape::None : found->second;
}

void moveEntityDown(MutablePacket& packet, std::int32_t kind) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    protocol::varInt::write(output, protocol::varInt::read(input));
    if (kind == 2) {
        const bool onGround = input.readByte() != 0;
        output.writeBytes(input.readSpan(2));
        protocol::writeBoolean(output, onGround);
    } else {
        const std::int32_t properties = protocol::varInt::read(input);
        const bool onGround = (properties & 1) != 0;
        const std::int32_t steps = properties >> 1;
        std::array<std::int32_t, 3> total{};
        for (std::int32_t step = 0; step < std::max(steps, 1) && !input.failed(); ++step) {
            if (steps > 0) {
                static_cast<void>(protocol::varInt::read(input));
            }
            for (std::int32_t& axis : total) {
                axis += input.readShort();
            }
        }
        for (const std::int32_t axis : total) {
            output.writeShort(static_cast<std::int16_t>(std::clamp(axis, -32768, 32767)));
        }
        if (kind == 1) {
            output.writeBytes(input.readSpan(2));
        }
        protocol::writeBoolean(output, onGround);
    }
    if (!input.failed()) {
        packet.body = output.toVector();
    }
}

void downgradeTrimMaterial(nbt::NbtCompound& material) {
    std::string palette{material.getString("palette_id")};
    material.remove("palette_id");
    if (const std::size_t colon = palette.find(':'); colon != std::string::npos) {
        palette.erase(0, colon + 1);
    }
    if (palette.starts_with(kTrimPalettePrefix)) {
        palette.erase(0, kTrimPalettePrefix.size());
    }
    if (std::ranges::find(kDarkerOnOwnArmour, palette) != kDarkerOnOwnArmour.end()) {
        nbt::NbtCompound overrides;
        overrides.putString("minecraft:" + palette, palette + "_darker");
        material.set("override_armor_assets", nbt::NbtTag::ofCompound(std::move(overrides)));
    }
    material.putString("asset_name", std::move(palette));
}

} // namespace

bool Link776::clientbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    if (state == ProtocolState::Configuration) {
        return configuration(packet);
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::LoginPlayPacket::kId: {
        const LoginFields login =
            decode(packet, [](io::ByteBuffer& in) { return LoginFields::read(in, true, true, true); });
        encode(packet, [&](io::ByteBuffer& out) { login.write(out, true, true); });
        break;
    }
    case play::RespawnPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            const LoginFields::SpawnFields spawn = LoginFields::SpawnFields::read(input, true, true);
            const std::uint8_t dataToKeep = input.readByte();
            spawn.write(output, true);
            output.writeByte(dataToKeep);
        });
        break;
    case play::AnimatePacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            protocol::varInt::write(output, protocol::varInt::read(input));
            // parity: 26.2's numbering, where 1 was the retired hurt animation.
            constexpr std::array<std::uint8_t, 3> kOlder = {2, 4, 5};
            const std::uint8_t action = input.readByte();
            output.writeByte(action < kOlder.size() ? kOlder[action] : action);
        });
        break;
    case play::SwingAnimationPacket::kId: {
        const auto swing = decode(packet, [](io::ByteBuffer& in) { return play::SwingAnimationPacket::read(in); });
        if (!swing.has_value() || swing->animation.type == play::SwingAnimationType::None) {
            return false;
        }
        packet.overrideId(kAnimate776);
        encode(packet, [&](io::ByteBuffer& out) {
            protocol::varInt::write(out, swing->entityId);
            out.writeByte(swing->hand == 0 ? kSwingMainHand776 : kSwingOffHand776);
        });
        break;
    }
    case play::EntityPositionSyncPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            protocol::varInt::write(output, protocol::varInt::read(input));
            std::array<double, 3> end{};
            std::array<double, 3> velocity{};
            if (protocol::varInt::read(input) == 0) {
                end = {input.readDouble(), input.readDouble(), input.readDouble()};
            } else {
                // Only the last step survives; its distance from the one before is the velocity.
                const std::int32_t steps = protocol::readCollectionSize(input, 25);
                std::array<double, 3> previous{};
                std::int32_t ticks = 0;
                for (std::int32_t i = 0; i < steps && !input.failed(); ++i) {
                    previous = end;
                    end = {input.readDouble(), input.readDouble(), input.readDouble()};
                    ticks = protocol::varInt::read(input);
                }
                if (steps > 1 && ticks > 0) {
                    for (std::size_t axis = 0; axis < 3; ++axis) {
                        velocity[axis] = (end[axis] - previous[axis]) / ticks;
                    }
                }
            }
            for (const double axis : end) {
                output.writeDouble(axis);
            }
            for (const double axis : velocity) {
                output.writeDouble(axis);
            }
            output.writeBytes(input.readable());
        });
        break;
    case kMoveEntityPos:
        moveEntityDown(packet, 0);
        break;
    case kMoveEntityPosRot:
        moveEntityDown(packet, 1);
        break;
    case kMoveEntityRot:
        moveEntityDown(packet, 2);
        break;
    case play::LevelParticlesPacket::kId:
        particles(packet);
        break;
    case play::ExplodePacket::kId:
        if (!packet.body.empty()) {
            packet.body.pop_back();
        }
        break;
    case play::OpenSignEditorPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            output.writeLong(input.readLong());
            protocol::writeBoolean(output, protocol::varInt::read(input) == 1);
        });
        break;
    case play::MapChunkPacket::kId: {
        const auto chunk = decode(packet, [](io::ByteBuffer& in) { return play::MapChunkPacket::read(in); });
        if (!chunk.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& out) {
            out.writeInt(chunk->x);
            out.writeInt(chunk->z);
            protocol::varInt::write(out, static_cast<std::int32_t>(chunk->heightmaps.size()));
            for (const play::Heightmap& heightmap : chunk->heightmaps) {
                heightmap.write(out);
            }
            protocol::writeByteArray(out, chunk->chunkData);
            protocol::varInt::write(out, static_cast<std::int32_t>(chunk->blockEntities.size()));
            for (const play::ChunkBlockEntityInfo& blockEntity : chunk->blockEntities) {
                blockEntity.write(out);
            }
            writeLongMaskLight(out, chunk->light);
        });
        break;
    }
    case play::UpdateLightPacket::kId: {
        const auto light = decode(packet, [](io::ByteBuffer& in) { return play::UpdateLightPacket::read(in); });
        if (!light.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& out) {
            protocol::varInt::write(out, light->chunkX);
            protocol::varInt::write(out, light->chunkZ);
            writeLongMaskLight(out, light->light);
        });
        break;
    }
    case play::OpenScreenPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t containerId = protocol::varInt::read(input);
        const std::int32_t menu = target_->menu.unmap(protocol::varInt::read(input));
        if (menu == play::menuType::kBrewingStand) {
            brewingStands_.insert(containerId);
        } else {
            brewingStands_.erase(containerId);
        }
        break;
    }
    case play::ContainerClosePacket::kClientboundId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        brewingStands_.erase(protocol::varInt::read(input));
        break;
    }
    case play::ContainerSetDataPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t containerId = protocol::varInt::read(input);
        if (brewingStands_.contains(containerId) && input.readShort() >= kFirstNewBrewingSlot) {
            return false;
        }
        break;
    }
    case play::CommandsPacket::kId: {
        auto commands = decode(packet, [](io::ByteBuffer& in) { return play::CommandsPacket::read(in); });
        if (!commands.has_value()) {
            return true;
        }
        for (play::CommandNodeData& node : commands->nodes) {
            if (node.parser.has_value()) {
                node.parser = olderArgument(*node.parser);
            }
        }
        encode(packet, [&](io::ByteBuffer& out) { commands->write(out); });
        break;
    }
    case play::UpdateRecipesPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            struct Set {
                std::string key;
                std::vector<std::int32_t> items;
            };
            std::vector<Set> kept;
            const std::int32_t sets = protocol::readCollectionSize(input, 2);
            for (std::int32_t i = 0; i < sets && !input.failed(); ++i) {
                Set set{protocol::readString(input), {}};
                const std::int32_t items = protocol::readCollectionSize(input);
                for (std::int32_t k = 0; k < items && !input.failed(); ++k) {
                    set.items.push_back(protocol::varInt::read(input));
                }
                if (std::ranges::find(kNewPropertySets, set.key) == kNewPropertySets.end()) {
                    kept.push_back(std::move(set));
                }
            }
            protocol::varInt::write(output, static_cast<std::int32_t>(kept.size()));
            for (const Set& set : kept) {
                protocol::writeString(output, set.key);
                protocol::varInt::write(output, static_cast<std::int32_t>(set.items.size()));
                for (const std::int32_t item : set.items) {
                    protocol::varInt::write(output, item);
                }
            }
            output.writeBytes(input.readable());
        });
        break;
    default:
        break;
    }
    return true;
}

bool Link776::configuration(MutablePacket& packet) const {
    if (packet.nativeId() == configuration::RegistryDataPacket::kId) {
        auto data = decode(packet, [](io::ByteBuffer& in) { return configuration::RegistryDataPacket::read(in); });
        if (!data.has_value()) {
            return true;
        }
        if (std::ranges::find(kNewRegistries, data->registryId) != kNewRegistries.end()) {
            return false;
        }
        if (data->registryId != "minecraft:trim_material") {
            return true;
        }
        for (configuration::RegistryEntry& entry : data->entries) {
            if (entry.value.has_value()) {
                if (nbt::NbtCompound* material = entry.value->get<nbt::NbtCompound>()) {
                    downgradeTrimMaterial(*material);
                }
            }
        }
        encode(packet, [&](io::ByteBuffer& out) { data->write(out); });
        return true;
    }
    if (packet.nativeId() == configuration::TagsPacket::kId) {
        auto tags = decode(packet, [](io::ByteBuffer& in) { return configuration::TagsPacket::read(in); });
        if (!tags.has_value()) {
            return true;
        }
        for (configuration::TagGroup& group : tags->groups) {
            if (group.registryId != "minecraft:block") {
                continue;
            }
            for (configuration::TagEntry& tag : group.tags) {
                if (tag.name == "minecraft:convertible_to_mud") {
                    tag.name = "minecraft:convertable_to_mud";
                }
            }
        }
        encode(packet, [&](io::ByteBuffer& out) { tags->write(out); });
    }
    return true;
}

void Link776::particles(MutablePacket& packet) const {
    rewrite(packet, [this](io::ByteBuffer& input, io::ByteBuffer& output) {
        const std::int32_t type = protocol::varInt::read(input);
        const Shape shape = shapeOf(target_->particleType.unmap(type));
        std::optional<play::ParticleOptions> options;
        if (shape != Shape::None) {
            options = play::ParticleOptions::read(input, shape);
            if (!options.has_value()) {
                return;
            }
        }
        output.writeBytes(input.readSpan(2));
        output.writeBytes(input.readSpan(3 * 8 + 3 * 4));
        output.writeFloat(input.readFloat());
        static_cast<void>(input.readFloat());
        static_cast<void>(input.readFloat());
        output.writeInt(protocol::varInt::read(input));
        protocol::varInt::write(output, type);
        if (options.has_value()) {
            options->write(output);
        }
        if (input.failed()) {
            output.clear();
        }
    });
}

bool Link776::serverbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::serverbound(state, packet)) {
        return false;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::kPunchId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        if (protocol::varInt::read(input) != 0) {
            return false;
        }
        packet.body.clear();
        break;
    }
    case play::ServerboundPlayerActionPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            const std::int32_t action = protocol::varInt::read(input);
            protocol::varInt::write(output, action >= kChangeDestroyDirection ? action + 1 : action);
            output.writeBytes(input.readable());
        });
        break;
    case play::AcceptTeleportationPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        pendingTeleport_ = protocol::varInt::read(input);
        return false;
    }
    case play::ServerboundPositionLookPacket::kId: {
        if (!pendingTeleport_.has_value()) {
            break;
        }
        const std::int32_t teleportId = *pendingTeleport_;
        pendingTeleport_.reset();
        packet.overrideId(play::AcceptTeleportationPacket::kId);
        rewrite(packet, [teleportId](io::ByteBuffer& input, io::ByteBuffer& output) {
            protocol::varInt::write(output, teleportId);
            output.writeBytes(input.readSpan(3 * 8 + 2 * 4));
        });
        break;
    }
    case play::SignUpdatePacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            output.writeLong(input.readLong());
            const bool front = input.readByte() != 0;
            for (std::size_t line = 0; line < play::SignUpdatePacket::kLines; ++line) {
                protocol::writeString(output, protocol::readString(input, play::SignUpdatePacket::kMaxLineLength));
            }
            protocol::varInt::write(output, front ? 1 : 0);
        });
        break;
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
