#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/multiversion/java/Link773Up.hpp>
#include <basalt/wire/multiversion/java/NewRegistries774.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>
#include <map>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
namespace configuration = protocol::configuration;
using protocol::ProtocolState;

constexpr std::int64_t kMillisPerTick = 50;

/// parity: ServerboundPlayerActionPacket.Action.STAB, which only 1.21.11 knows.
constexpr std::int32_t kStab = 7;

std::string hexColor(std::int32_t rgb) {
    constexpr char kDigits[] = "0123456789abcdef";
    std::string text = "#";
    for (int shift = 20; shift >= 0; shift -= 4) {
        text += kDigits[(static_cast<std::uint32_t>(rgb) >> static_cast<unsigned>(shift)) & 0xFU];
    }
    return text;
}

nbt::NbtTag colourOf(const nbt::NbtTag& tag) {
    const auto* value = tag.get<std::int32_t>();
    return value == nullptr ? tag : nbt::NbtTag::ofString(hexColor(*value));
}

nbt::NbtTag newerBiome(const nbt::NbtTag& value) {
    const nbt::NbtCompound* biome = value.get<nbt::NbtCompound>();
    if (biome == nullptr) {
        return value;
    }
    const nbt::NbtCompound* old = biome->getCompound("effects");
    nbt::NbtCompound rewritten;
    for (std::size_t i = 0; i < biome->size(); ++i) {
        if (biome->names()[i] != "effects") {
            rewritten.set(biome->names()[i], biome->values()[i]);
        }
    }
    nbt::NbtCompound effects;
    nbt::NbtCompound attributes;
    if (old != nullptr) {
        for (std::size_t i = 0; i < old->size(); ++i) {
            const std::string& name = old->names()[i];
            const nbt::NbtTag& field = old->values()[i];
            if (name == "fog_color") {
                attributes.set("minecraft:visual/fog_color", colourOf(field));
            } else if (name == "water_fog_color") {
                attributes.set("minecraft:visual/water_fog_color", colourOf(field));
            } else if (name == "sky_color") {
                attributes.set("minecraft:visual/sky_color", colourOf(field));
            } else if (name == "mood_sound") {
                nbt::NbtCompound ambient;
                ambient.set("mood", field);
                attributes.set("minecraft:audio/ambient_sounds", nbt::NbtTag::ofCompound(std::move(ambient)));
            } else if (name == "music") {
                const nbt::NbtList* list = field.get<nbt::NbtList>();
                if (list != nullptr && !list->empty()) {
                    if (const nbt::NbtCompound* first = (*list)[0].get<nbt::NbtCompound>()) {
                        if (const nbt::NbtTag* data = first->find("data")) {
                            nbt::NbtCompound music;
                            music.set("default", *data);
                            attributes.set("minecraft:audio/background_music",
                                           nbt::NbtTag::ofCompound(std::move(music)));
                        }
                    }
                }
            } else if (name == "water_color" || name == "foliage_color" || name == "dry_foliage_color" ||
                       name == "grass_color") {
                effects.set(name, colourOf(field));
            } else if (name != "music_volume") {
                effects.set(name, field);
            }
        }
    }
    rewritten.set("attributes", nbt::NbtTag::ofCompound(std::move(attributes)));
    rewritten.set("effects", nbt::NbtTag::ofCompound(std::move(effects)));
    return nbt::NbtTag::ofCompound(std::move(rewritten));
}

const std::map<std::string, nbt::NbtTag>& vanillaDimensions() {
    static const std::map<std::string, nbt::NbtTag> dimensions = [] {
        std::map<std::string, nbt::NbtTag> parsed;
        const std::span<const std::uint8_t> body = dimensionTypesOf1_21_11();
        io::ByteBuffer input = io::ByteBuffer::wrap(std::vector<std::uint8_t>{body.begin(), body.end()});
        if (const auto data = configuration::RegistryDataPacket::read(input)) {
            for (const configuration::RegistryEntry& entry : data->entries) {
                if (entry.value.has_value()) {
                    parsed.emplace(entry.key, *entry.value);
                }
            }
        }
        return parsed;
    }();
    return dimensions;
}

std::int32_t idAt(ProtocolState state, bool clientbound, std::int32_t nativeId) {
    return javaTranslators::packetIdAt(protocolVersions::kV1_21_9, state, clientbound, nativeId);
}

} // namespace

bool Link773Up::configuration(MutablePacket& packet) const {
    static const std::int32_t registryData =
        idAt(ProtocolState::Configuration, true, configuration::RegistryDataPacket::kId);
    static const std::int32_t finish = idAt(ProtocolState::Configuration, true, configuration::kFinishConfigurationId);
    if (packet.id == registryData) {
        auto data = decode(packet, [](io::ByteBuffer& in) { return configuration::RegistryDataPacket::read(in); });
        if (!data.has_value()) {
            return true;
        }
        if (data->registryId == "minecraft:dimension_type") {
            const auto& vanilla = vanillaDimensions();
            for (configuration::RegistryEntry& entry : data->entries) {
                auto found = vanilla.find(entry.key);
                if (found == vanilla.end()) {
                    found = vanilla.find("minecraft:overworld");
                }
                if (found != vanilla.end()) {
                    entry.value = found->second;
                }
            }
        } else if (data->registryId == "minecraft:damage_type") {
            // The spear's damage type is named by item components the client builds on its own.
            const bool present = std::ranges::any_of(data->entries, [](const configuration::RegistryEntry& entry) {
                return entry.key == "minecraft:spear";
            });
            if (!present) {
                nbt::NbtCompound spear;
                spear.putFloat("exhaustion", 0.1F);
                spear.putString("message_id", "spear");
                spear.putString("scaling", "when_caused_by_living_non_player");
                data->entries.push_back({"minecraft:spear", nbt::NbtTag::ofCompound(std::move(spear))});
            }
        } else if (data->registryId == "minecraft:worldgen/biome") {
            for (configuration::RegistryEntry& entry : data->entries) {
                if (entry.value.has_value()) {
                    entry.value = newerBiome(*entry.value);
                }
            }
        } else {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& out) { data->write(out); });
    } else if (packet.id == finish) {
        const auto added = registriesAddedIn1_21_11();
        const std::vector<std::uint8_t> finishBody = packet.body;
        packet.overrideId(configuration::RegistryDataPacket::kId);
        packet.body.assign(added[0].begin(), added[0].end());
        for (std::size_t index = 1; index < added.size(); ++index) {
            packet.extras.emplace_back(registryData,
                                       std::vector<std::uint8_t>{added[index].begin(), added[index].end()});
        }
        packet.extras.emplace_back(finish, finishBody);
    }
    return true;
}

bool Link773Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    if (state == ProtocolState::Configuration) {
        return configuration(packet);
    }
    static const std::int32_t lerp = idAt(ProtocolState::Play, true, play::SetBorderLerpSizePacket::kId);
    static const std::int32_t initialize = idAt(ProtocolState::Play, true, play::InitializeBorderPacket::kId);
    if (state == ProtocolState::Play && packet.id == lerp) {
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            auto border = play::SetBorderLerpSizePacket::read(input);
            if (border.has_value()) {
                border->lerpTimeTicks /= kMillisPerTick;
                border->write(output);
            }
        });
    } else if (state == ProtocolState::Play && packet.id == initialize) {
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            auto border = play::InitializeBorderPacket::read(input);
            if (border.has_value()) {
                border->lerpTimeTicks /= kMillisPerTick;
                border->write(output);
            }
        });
    }
    return true;
}

bool Link773Up::serverbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::serverbound(state, packet)) {
        return false;
    }
    static const std::int32_t playerAction = idAt(ProtocolState::Play, false, play::ServerboundPlayerActionPacket::kId);
    if (state == ProtocolState::Play && packet.nativeId() == playerAction) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        if (protocol::varInt::read(input) == kStab) {
            return false;
        }
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
