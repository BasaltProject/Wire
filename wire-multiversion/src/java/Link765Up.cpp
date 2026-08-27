#include <basalt/wire/multiversion/java/CommandParsers.hpp>
#include <basalt/wire/multiversion/java/Link765Up.hpp>
#include <basalt/wire/multiversion/java/NewRegistries766.hpp>
#include <basalt/wire/multiversion/java/Stack765.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;
using protocol::ProtocolState;

constexpr std::int32_t kCommands = 17;
constexpr std::int32_t kLoginGameProfile = 2;
constexpr std::int32_t kConfigRegistryData = 5;
constexpr std::int32_t kConfigRegistryData766 = 7;

constexpr std::int32_t kContainerContent = 19;
constexpr std::int32_t kContainerSlot = 21;
constexpr std::int32_t kLevelParticles = 39;
constexpr std::int32_t kLogin = 41;
constexpr std::int32_t kMerchantOffers = 43;
constexpr std::int32_t kRespawn = 69;
constexpr std::int32_t kServerData = 73;
constexpr std::int32_t kSetEquipment = 89;
constexpr std::int32_t kUpdateAdvancements = 112;
constexpr std::int32_t kUpdateAttributes = 113;
constexpr std::int32_t kUpdateMobEffect = 114;

constexpr std::int32_t kSetCreativeSlot = 47;
constexpr std::int32_t kMoreSlots = 0x80;

constexpr std::int32_t kChatCommand765 = 4;
constexpr std::int32_t kChatCommandSigned766 = 5;
constexpr std::int32_t kLastParser765 = 49;
constexpr std::array<std::int32_t, kLastParser765 + 1> kParsersIn766 = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 53};

std::int32_t parserIn766(std::int32_t parser) {
    return parser >= 0 && parser <= kLastParser765 ? kParsersIn766[static_cast<std::size_t>(parser)] : parser;
}

constexpr std::array<std::string_view, 22> kAttributes = {{"minecraft:generic.armor",
                                                           "minecraft:generic.armor_toughness",
                                                           "minecraft:generic.attack_damage",
                                                           "minecraft:generic.attack_knockback",
                                                           "minecraft:generic.attack_speed",
                                                           "minecraft:player.block_break_speed",
                                                           "minecraft:player.block_interaction_range",
                                                           "minecraft:player.entity_interaction_range",
                                                           "minecraft:generic.fall_damage_multiplier",
                                                           "minecraft:generic.flying_speed",
                                                           "minecraft:generic.follow_range",
                                                           "minecraft:generic.gravity",
                                                           "minecraft:generic.jump_strength",
                                                           "minecraft:generic.knockback_resistance",
                                                           "minecraft:generic.luck",
                                                           "minecraft:generic.max_absorption",
                                                           "minecraft:generic.max_health",
                                                           "minecraft:generic.movement_speed",
                                                           "minecraft:generic.safe_fall_distance",
                                                           "minecraft:generic.scale",
                                                           "minecraft:zombie.spawn_reinforcements",
                                                           "minecraft:generic.step_height"}};

std::int32_t attributeId(std::string_view name) {
    const auto found = std::ranges::find(kAttributes, name);
    return found == kAttributes.end() ? -1 : static_cast<std::int32_t>(found - kAttributes.begin());
}

void copyStack(io::ByteBuffer& in, io::ByteBuffer& out) {
    if (!stack765::toComponents(in, out)) {
        in.markFailed();
    }
}

void rewriteContainerSlot(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    out.writeByte(in.readByte());
    pv::varInt::write(out, pv::varInt::read(in));
    out.writeShort(in.readShort());
    copyStack(in, out);
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteContainerContent(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    out.writeByte(in.readByte());
    pv::varInt::write(out, pv::varInt::read(in));
    const std::int32_t count = pv::varInt::read(in);
    pv::varInt::write(out, count);
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        copyStack(in, out);
    }
    copyStack(in, out);
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteEquipment(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    pv::varInt::write(out, pv::varInt::read(in));
    std::uint8_t slot = 0;
    do {
        slot = in.readByte();
        out.writeByte(slot);
        copyStack(in, out);
    } while ((slot & kMoreSlots) != 0 && !in.failed());
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteMerchantOffers(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    pv::varInt::write(out, pv::varInt::read(in));
    const std::int32_t trades = pv::varInt::read(in);
    pv::varInt::write(out, trades);
    for (std::int32_t index = 0; index < trades && !in.failed(); ++index) {
        copyStack(in, out);
        copyStack(in, out);
        const bool second = pv::readBoolean(in);
        pv::writeBoolean(out, second);
        if (second) {
            copyStack(in, out);
        }
        out.writeBytes(in.readSpan(1 + 4 * 5 + 4));
    }
    out.writeBytes(in.readable());
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteMobEffect(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    pv::varInt::write(out, pv::varInt::read(in));
    pv::varInt::write(out, pv::varInt::read(in));
    pv::varInt::write(out, static_cast<std::int8_t>(in.readByte()));
    pv::varInt::write(out, pv::varInt::read(in));
    out.writeByte(in.readByte());
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteAttributes(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer body;
    const std::int32_t entity = pv::varInt::read(in);
    const std::int32_t count = pv::varInt::read(in);
    std::int32_t kept = 0;
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        const std::int32_t id = attributeId(pv::readString(in));
        const double base = in.readDouble();
        const std::int32_t modifiers = pv::varInt::read(in);
        const std::span<const std::uint8_t> modifierBytes = in.readSpan(static_cast<std::size_t>(modifiers) * 25);
        if (id < 0 || in.failed()) {
            continue;
        }
        ++kept;
        pv::varInt::write(body, id);
        body.writeDouble(base);
        pv::varInt::write(body, modifiers);
        body.writeBytes(modifierBytes);
    }
    if (in.failed()) {
        return;
    }
    io::ByteBuffer out;
    pv::varInt::write(out, entity);
    pv::varInt::write(out, kept);
    out.writeBytes(body.readable());
    packet.body = out.toVector();
}

void fixDimensionType(nbt::NbtCompound& element) {
    nbt::NbtTag* light = element.find("monster_spawn_light_level");
    auto* provider = light != nullptr ? light->get<nbt::NbtCompound>() : nullptr;
    nbt::NbtTag* value = provider != nullptr ? provider->find("value") : nullptr;
    auto* range = value != nullptr ? value->get<nbt::NbtCompound>() : nullptr;
    if (range == nullptr) {
        return;
    }
    const std::int32_t low = range->getInt("min_inclusive", 0);
    const std::int32_t high = range->getInt("max_inclusive", 7);
    provider->remove("value");
    provider->putInt("min_inclusive", low);
    provider->putInt("max_inclusive", high);
}

} // namespace

bool Link765Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Login) {
        if (packet.id == kLoginGameProfile) {
            packet.body.push_back(0);
        }
        return true;
    }
    if (state == ProtocolState::Configuration) {
        if (packet.id != kConfigRegistryData) {
            return true;
        }
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        const nbt::NbtTag root = pv::readAnonymousNbt(in);
        const auto* registries = root.get<nbt::NbtCompound>();
        if (in.failed() || registries == nullptr) {
            return true;
        }
        std::vector<protocol::configuration::RegistryDataPacket> split;
        for (const std::string& key : registries->names()) {
            const auto* registry = registries->find(key)->get<nbt::NbtCompound>();
            const nbt::NbtList* values = registry != nullptr ? registry->getList("value") : nullptr;
            if (values == nullptr) {
                continue;
            }
            std::vector<std::pair<std::int32_t, protocol::configuration::RegistryEntry>> ordered;
            for (const nbt::NbtTag& each : values->elements()) {
                const auto* compound = each.get<nbt::NbtCompound>();
                const nbt::NbtTag* element = compound != nullptr ? compound->find("element") : nullptr;
                const nbt::NbtTag* name = compound != nullptr ? compound->find("name") : nullptr;
                const std::string* nameText = name != nullptr ? name->get<std::string>() : nullptr;
                if (nameText == nullptr) {
                    continue;
                }
                nbt::NbtTag copy = element != nullptr ? *element : nbt::NbtTag::ofCompound(nbt::NbtCompound{});
                if (key == "minecraft:dimension_type") {
                    if (auto* dimension = copy.get<nbt::NbtCompound>()) {
                        fixDimensionType(*dimension);
                    }
                }
                ordered.emplace_back(compound->getInt("id", 0),
                                     protocol::configuration::RegistryEntry{*nameText, std::move(copy)});
            }
            std::ranges::stable_sort(ordered, {}, &decltype(ordered)::value_type::first);
            protocol::configuration::RegistryDataPacket data;
            data.registryId = key;
            for (auto& [id, entry] : ordered) {
                if (key == "minecraft:dimension_type") {
                    dimensionIds_[entry.key] = static_cast<std::int32_t>(data.entries.size());
                }
                data.entries.push_back(std::move(entry));
            }
            split.push_back(std::move(data));
        }
        std::vector<std::vector<std::uint8_t>> bodies;
        for (const protocol::configuration::RegistryDataPacket& data : split) {
            io::ByteBuffer out;
            data.write(out);
            bodies.push_back(out.toVector());
        }
        for (const auto& added : registriesAddedIn1_20_5()) {
            bodies.emplace_back(added.begin(), added.end());
        }
        if (bodies.empty()) {
            return false;
        }
        packet.overrideId(kConfigRegistryData766);
        packet.body = std::move(bodies.front());
        for (std::size_t index = 1; index < bodies.size(); ++index) {
            packet.forwarded.emplace_back(kConfigRegistryData766, std::move(bodies[index]));
        }
        return true;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kLogin: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer out;
        out.writeBytes(in.readSpan(4 + 1));
        const std::int32_t worlds = pv::varInt::read(in);
        pv::varInt::write(out, worlds);
        for (std::int32_t index = 0; index < worlds && !in.failed(); ++index) {
            pv::writeString(out, pv::readString(in));
        }
        for (int index = 0; index < 3; ++index) {
            pv::varInt::write(out, pv::varInt::read(in));
        }
        out.writeBytes(in.readSpan(3));
        const std::string dimension = pv::readString(in);
        const auto found = dimensionIds_.find(dimension);
        pv::varInt::write(out, found == dimensionIds_.end() ? 0 : found->second);
        pv::writeString(out, pv::readString(in));
        out.writeBytes(in.readSpan(8 + 2 + 2));
        const bool death = pv::readBoolean(in);
        pv::writeBoolean(out, death);
        if (death) {
            pv::writeString(out, pv::readString(in));
            out.writeBytes(in.readSpan(8));
        }
        pv::varInt::write(out, pv::varInt::read(in));
        pv::writeBoolean(out, false);
        if (!in.failed()) {
            packet.body = out.toVector();
        }
        break;
    }
    case kRespawn: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer out;
        const std::string dimension = pv::readString(in);
        const auto found = dimensionIds_.find(dimension);
        pv::varInt::write(out, found == dimensionIds_.end() ? 0 : found->second);
        out.writeBytes(in.readable());
        if (!in.failed()) {
            packet.body = out.toVector();
        }
        break;
    }
    case kServerData:
        if (!packet.body.empty()) {
            packet.body.pop_back();
        }
        break;
    case kUpdateMobEffect:
        rewriteMobEffect(packet);
        break;
    case kUpdateAttributes:
        rewriteAttributes(packet);
        break;
    case kContainerSlot:
        rewriteContainerSlot(packet);
        break;
    case kContainerContent:
        rewriteContainerContent(packet);
        break;
    case kSetEquipment:
        rewriteEquipment(packet);
        break;
    case kMerchantOffers:
        rewriteMerchantOffers(packet);
        break;
    case kCommands: {
        commandParsers::Layout layout;
        layout.scoreHolder = 30;
        layout.time = 41;
        layout.firstRegistry = 42;
        layout.lastRegistry = 45;
        static_cast<void>(commandParsers::remap(packet, layout, parserIn766));
        break;
    }
    case kUpdateAdvancements:
    case kLevelParticles:
        return false;
    default:
        break;
    }
    return true;
}

bool Link765Up::serverboundForeign(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play || packet.id != kChatCommandSigned766) {
        return false;
    }
    packet.id = kChatCommand765;
    signedCommand_ = true;
    return true;
}

bool Link765Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Play && packet.id == kChatCommand765) {
        if (signedCommand_) {
            signedCommand_ = false;
            return true;
        }
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        const std::string command = pv::readString(in);
        if (in.failed()) {
            return true;
        }
        io::ByteBuffer out;
        pv::writeString(out, command);
        out.writeLong(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
        out.writeLong(0);
        pv::varInt::write(out, 0);
        pv::varInt::write(out, 0);
        out.writeBytes(std::array<std::uint8_t, 3>{});
        packet.body = out.toVector();
        return true;
    }
    if (state != ProtocolState::Play || packet.id != kSetCreativeSlot) {
        return true;
    }
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int16_t slot = in.readShort();
    const std::int32_t count = pv::varInt::read(in);
    io::ByteBuffer out;
    out.writeShort(slot);
    if (count <= 0 || in.failed()) {
        pv::writeBoolean(out, false);
    } else {
        const std::int32_t item = pv::varInt::read(in);
        pv::writeBoolean(out, true);
        pv::varInt::write(out, item);
        out.writeByte(static_cast<std::uint8_t>(count));
        out.writeByte(0);
    }
    packet.body = out.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
