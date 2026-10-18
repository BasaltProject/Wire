#include <basalt/wire/multiversion/java/Link774Up.hpp>
#include <basalt/wire/multiversion/java/NewRegistries775.hpp>
#include <basalt/wire/multiversion/java/Tables774.hpp>
#include <basalt/wire/multiversion/java/Tables775.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/LowPrecisionVec3.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
namespace configuration = protocol::configuration;
using protocol::ProtocolState;

constexpr std::int32_t kSetTime775 = 113;
constexpr std::int32_t kRespawn775 = 82;
constexpr std::int32_t kPlayerInput775 = 43;
constexpr std::int32_t kInteract775 = 26;
constexpr std::int32_t kAttack775 = 1;
constexpr std::int32_t kSpectateEntity775 = 62;
constexpr std::int32_t kClientCommand775 = 12;

constexpr std::int32_t kInteract = 0;
constexpr std::int32_t kAttack = 1;
constexpr std::int32_t kInteractAt = 2;
constexpr std::uint8_t kSneakBit = 1U << 5U;

/// parity: ServerboundClientCommandPacket.Action.REQUEST_GAMERULE_VALUES, which 1.21.11 has no counterpart for.
constexpr std::int32_t kRequestGameRules = 2;

std::string_view withoutNamespace(std::string_view value) {
    const std::size_t colon = value.find(':');
    return colon == std::string_view::npos ? value : value.substr(colon + 1);
}

std::string swapAffix(std::string asset, std::string_view kind) {
    const std::string suffix = "_" + std::string{kind};
    if (!asset.ends_with(suffix)) {
        return asset;
    }
    asset.erase(asset.size() - suffix.size());
    const std::string folder = std::string{kind} + "/";
    const std::size_t at = asset.find(folder);
    if (at != std::string::npos) {
        asset.insert(at + folder.size(), std::string{kind} + "_");
    }
    return asset;
}

std::string prefixName(std::string asset, std::string_view kind) {
    const std::string folder = std::string{kind} + "/";
    const std::size_t at = asset.find(folder);
    if (at != std::string::npos) {
        asset.insert(at + folder.size(), std::string{kind} + "_");
    }
    return asset;
}

void addBabyAsset(nbt::NbtCompound& element) {
    element.putString("baby_asset_id", std::string{element.getString("asset_id")} + "_baby");
}

int ambientLightColor(float ambientLight) {
    // Between the overworld (no ambient light) and full white.
    const int value = std::clamp(static_cast<int>(std::lround(10.0F + ambientLight * 245.0F)), 10, 255);
    return static_cast<int>(0xFF000000U | static_cast<unsigned>(value) << 16U | static_cast<unsigned>(value) << 8U |
                            static_cast<unsigned>(value));
}

/// parity: the markers the overworld day timeline carries from 26.1, the ones `/time set` and sleeping read.
nbt::NbtCompound overworldTimeMarkers() {
    const auto shown = [](std::int32_t ticks) {
        nbt::NbtCompound marker;
        marker.putBoolean("show_in_commands", true);
        marker.putInt("ticks", ticks);
        return nbt::NbtTag::ofCompound(std::move(marker));
    };
    nbt::NbtCompound markers;
    markers.set("minecraft:day", shown(1000));
    markers.set("minecraft:noon", shown(6000));
    markers.set("minecraft:midnight", shown(18000));
    markers.set("minecraft:night", shown(13000));
    markers.putInt("minecraft:wake_up_from_sleep", 0);
    markers.putInt("minecraft:roll_village_siege", 18000);
    return markers;
}

std::string hexColor(int argb) {
    constexpr char kDigits[] = "0123456789abcdef";
    const auto rgb = static_cast<unsigned>(argb) & 0xFFFFFFU;
    std::string text = "#";
    for (int shift = 20; shift >= 0; shift -= 4) {
        text += kDigits[(rgb >> static_cast<unsigned>(shift)) & 0xFU];
    }
    return text;
}

void upgradeEntry(const std::string& registry, const std::string& key, nbt::NbtCompound& element) {
    if (registry == "minecraft:timeline") {
        element.putString("clock", "minecraft:overworld");
        if (key == "minecraft:day") {
            element.set("time_markers", nbt::NbtTag::ofCompound(overworldTimeMarkers()));
        }
    } else if (registry == "minecraft:wolf_sound_variant") {
        nbt::NbtCompound adult;
        adult.putString("step_sound", "minecraft:entity.wolf.step");
        nbt::NbtCompound baby;
        baby.putString("step_sound", "minecraft:entity.baby_wolf.step");
        for (std::size_t i = 0; i < element.size(); ++i) {
            adult.set(element.names()[i], element.values()[i]);
            const std::string* sound = element.values()[i].get<std::string>();
            const std::string text = sound == nullptr ? std::string{} : *sound;
            baby.putString(element.names()[i], "minecraft:entity.baby_wolf." + text.substr(text.rfind('.') + 1));
        }
        nbt::NbtCompound upgraded;
        upgraded.set("adult_sounds", nbt::NbtTag::ofCompound(std::move(adult)));
        upgraded.set("baby_sounds", nbt::NbtTag::ofCompound(std::move(baby)));
        element = std::move(upgraded);
    } else if (registry == "minecraft:wolf_variant") {
        if (const nbt::NbtCompound* assets = element.getCompound("assets")) {
            nbt::NbtCompound baby;
            for (std::size_t i = 0; i < assets->size(); ++i) {
                const std::string* value = assets->values()[i].get<std::string>();
                baby.putString(assets->names()[i], (value == nullptr ? std::string{} : *value) + "_baby");
            }
            element.set("baby_assets", nbt::NbtTag::ofCompound(std::move(baby)));
        }
    } else if (registry == "minecraft:frog_variant") {
        element.putString("asset_id", swapAffix(std::string{element.getString("asset_id")}, "frog"));
    } else if (registry == "minecraft:chicken_variant" || registry == "minecraft:cow_variant" ||
               registry == "minecraft:pig_variant") {
        const std::string kind{withoutNamespace(registry).substr(0, withoutNamespace(registry).find('_'))};
        element.putString("asset_id", swapAffix(std::string{element.getString("asset_id")}, kind));
        addBabyAsset(element);
    } else if (registry == "minecraft:cat_variant") {
        element.putString("asset_id", prefixName(std::string{element.getString("asset_id")}, "cat"));
        addBabyAsset(element);
    } else if (registry == "minecraft:dimension_type") {
        const std::string_view name = withoutNamespace(key);
        element.putBoolean("has_ender_dragon_fight", name == "the_end");
        if (name == "overworld" || name == "overworld_caves") {
            element.putString("default_clock", "minecraft:overworld");
        } else if (name == "the_end") {
            element.putString("default_clock", "minecraft:the_end");
        }
        nbt::NbtCompound attributes;
        if (const nbt::NbtCompound* present = element.getCompound("attributes")) {
            attributes = *present;
        }
        const int color = name == "the_end"      ? -12630209
                          : name == "the_nether" ? -13621215
                          : name == "overworld"  ? -16119286
                                                 : ambientLightColor(element.getFloat("ambient_light", 0.0F));
        attributes.putString("minecraft:visual/ambient_light_color", hexColor(color));
        element.set("attributes", nbt::NbtTag::ofCompound(std::move(attributes)));
    }
}

std::int32_t inStep(protocol::ProtocolState state, bool clientbound, std::int32_t id775) {
    const PacketIdTables& packets = tables775().packets;
    (void)packets;
    const PacketIdTables& step = tables774().packets;
    return clientbound ? step.toOlder(state, id775) : step.serverboundToOlder(state, id775);
}

} // namespace

bool Link774Up::serverboundForeign(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return false;
    }
    const bool attack = packet.id == kAttack775;
    const bool spectate = packet.id == kSpectateEntity775;
    if (!attack && !spectate) {
        return false;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    if (input.failed()) {
        return false;
    }
    packet.id = inStep(state, false, kInteract775);
    io::ByteBuffer out;
    protocol::varInt::write(out, entityId);
    protocol::varInt::write(out, kAttack);
    protocol::writeBoolean(out, sneaking_);
    packet.body = out.toVector();
    return true;
}

bool Link774Up::configuration(MutablePacket& packet) const {
    static const std::int32_t registryData = inStep(ProtocolState::Configuration, true, configuration::RegistryDataPacket::kId);
    static const std::int32_t finish = inStep(ProtocolState::Configuration, true, configuration::kFinishConfigurationId);
    if (packet.id == registryData) {
        auto data = decode(packet, [](io::ByteBuffer& in) { return configuration::RegistryDataPacket::read(in); });
        if (!data.has_value()) {
            return true;
        }
        bool changed = false;
        for (configuration::RegistryEntry& entry : data->entries) {
            if (!entry.value.has_value()) {
                continue;
            }
            if (nbt::NbtCompound* element = entry.value->get<nbt::NbtCompound>()) {
                upgradeEntry(data->registryId, entry.key, *element);
                changed = true;
            }
        }
        if (changed) {
            encode(packet, [&](io::ByteBuffer& out) { data->write(out); });
        }
    } else if (packet.id == finish) {
        const auto added = registriesAddedIn26_1();
        if (added.empty()) {
            return true;
        }
        const std::vector<std::uint8_t> finish_body = packet.body;
        packet.overrideId(configuration::RegistryDataPacket::kId);
        packet.body.assign(added[0].begin(), added[0].end());
        for (std::size_t index = 1; index < added.size(); ++index) {
            packet.extras.emplace_back(registryData,
                                       std::vector<std::uint8_t>{added[index].begin(), added[index].end()});
        }
        packet.extras.emplace_back(finish, finish_body);
    }
    return true;
}

bool Link774Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    if (state == ProtocolState::Configuration) {
        return configuration(packet);
    }
    static const std::int32_t setTime = inStep(ProtocolState::Play, true, kSetTime775);
    static const std::int32_t respawn = inStep(ProtocolState::Play, true, kRespawn775);
    if (state == ProtocolState::Play && packet.id == setTime) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int64_t gameTime = input.readLong();
        const std::int64_t dayTime = input.readLong();
        const bool ticking = input.readByte() != 0;
        if (input.failed()) {
            return true;
        }
        play::SetTimePacket upgraded;
        upgraded.gameTime = gameTime;
        for (const std::int32_t clock : {play::SetTimePacket::kOverworldClock, play::SetTimePacket::kEndClock}) {
            upgraded.clocks.push_back(play::ClockUpdate{.clockId = clock,
                                                        .totalTicks = dayTime,
                                                        .partialTick = 0.0F,
                                                        .rate = ticking ? 1.0F : 0.0F});
        }
        encode(packet, [&](io::ByteBuffer& out) { upgraded.write(out); });
    } else if (state == ProtocolState::Play && packet.id == respawn) {
        sneaking_ = false;
    }
    return true;
}

bool Link774Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::serverbound(state, packet)) {
        return false;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    static const std::int32_t playerInput = inStep(ProtocolState::Play, false, kPlayerInput775);
    static const std::int32_t interactId = inStep(ProtocolState::Play, false, kInteract775);
    static const std::int32_t clientCommand = inStep(ProtocolState::Play, false, kClientCommand775);
    const std::int32_t id = packet.nativeId();
    if (id == playerInput) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        sneaking_ = (input.readByte() & kSneakBit) != 0;
    } else if (id == interactId) {
        auto interact = decode(packet, [](io::ByteBuffer& in) { return play::InteractPacket::read(in); });
        if (!interact.has_value()) {
            return false;
        }
        sneaking_ = interact->usingSecondaryAction;
        // The plain interaction first, then the one with the point the player aimed at.
        encode(packet, [&](io::ByteBuffer& out) {
            protocol::varInt::write(out, interact->entityId);
            protocol::varInt::write(out, kInteract);
            protocol::varInt::write(out, interact->hand);
            protocol::writeBoolean(out, interact->usingSecondaryAction);
        });
        io::ByteBuffer at;
        protocol::varInt::write(at, interact->entityId);
        protocol::varInt::write(at, kInteractAt);
        at.writeFloat(static_cast<float>(interact->x));
        at.writeFloat(static_cast<float>(interact->y));
        at.writeFloat(static_cast<float>(interact->z));
        protocol::varInt::write(at, interact->hand);
        protocol::writeBoolean(at, interact->usingSecondaryAction);
        packet.extras.emplace_back(interactId, at.toVector());
    } else if (id == clientCommand) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        if (protocol::varInt::read(input) == kRequestGameRules) {
            return false;
        }
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
