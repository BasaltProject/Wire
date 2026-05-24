#include <basalt/wire/multiversion/java/Link773.hpp>
#include <basalt/wire/common/SafeInt.hpp>
#include <basalt/wire/nbt/Snbt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

constexpr std::string_view kDimensionType = "minecraft:dimension_type";
constexpr std::string_view kBiome = "minecraft:worldgen/biome";
constexpr std::string_view kOverworld = "minecraft:overworld";
constexpr std::string_view kTemperature = "temperature";

constexpr std::string_view kVisualFog = "minecraft:visual/fog_color";
constexpr std::string_view kVisualSky = "minecraft:visual/sky_color";
constexpr std::string_view kVisualWaterFog = "minecraft:visual/water_fog_color";
constexpr std::string_view kAudioAmbient = "minecraft:audio/ambient_sounds";
constexpr std::string_view kAudioMusic = "minecraft:audio/background_music";

constexpr std::string_view kFog = "fog_color";
constexpr std::string_view kWater = "water_color";
constexpr std::string_view kWaterFog = "water_fog_color";
constexpr std::string_view kSky = "sky_color";

/// parity: `BiomeSpecialEffects`' own defaults, which is what a biome that says nothing had.
constexpr std::int32_t kDefaultFog = 12638463;
constexpr std::int32_t kDefaultWater = 4159204;
constexpr std::int32_t kDefaultWaterFog = 329011;

constexpr std::int64_t kMillisPerTick = 50;

[[nodiscard]] std::optional<std::int32_t> colourOf(const nbt::NbtTag* tag) {
    if (tag == nullptr) {
        return std::nullopt;
    }
    const auto* text = tag->get<std::string>();
    if (text == nullptr || !text->starts_with('#')) {
        return std::nullopt;
    }
    std::int64_t value = 0;
    const char* first = text->data() + 1;
    const char* last = text->data() + text->size();
    const auto [end, error] = std::from_chars(first, last, value, 16);
    if (error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(value & 0xFFFFFF);
}

[[nodiscard]] std::optional<std::int32_t> colourFrom(const nbt::NbtCompound* attributes, std::string_view key) {
    return attributes == nullptr ? std::nullopt : colourOf(attributes->find(key));
}

[[nodiscard]] std::optional<std::int32_t> intOf(const nbt::NbtCompound& compound, std::string_view key) {
    const nbt::NbtTag* tag = compound.find(key);
    if (tag == nullptr) {
        return std::nullopt;
    }
    const auto* value = tag->get<std::int32_t>();
    return value == nullptr ? std::nullopt : std::optional{*value};
}

[[nodiscard]] std::int32_t hsvToRgb(float hue, float saturation, float value) {
    const int sector = static_cast<int>(common::safeInt(hue * 6.0F) % 6);
    const float fraction = hue * 6.0F - static_cast<float>(sector);
    const float p = value * (1.0F - saturation);
    const float q = value * (1.0F - fraction * saturation);
    const float t = value * (1.0F - (1.0F - fraction) * saturation);
    float red = value;
    float green = p;
    float blue = q;
    switch (sector) {
    case 0:
        red = value;
        green = t;
        blue = p;
        break;
    case 1:
        red = q;
        green = value;
        blue = p;
        break;
    case 2:
        red = p;
        green = value;
        blue = t;
        break;
    case 3:
        red = p;
        green = q;
        blue = value;
        break;
    case 4:
        red = t;
        green = p;
        blue = value;
        break;
    default:
        break;
    }
    return (static_cast<std::int32_t>(common::safeInt(red * 255.0F) & 255) << 16) |
           (static_cast<std::int32_t>(common::safeInt(green * 255.0F) & 255) << 8) |
           static_cast<std::int32_t>(common::safeInt(blue * 255.0F) & 255);
}

/// parity: `OverworldBiomeBuilder.calculateSkyColor`.
[[nodiscard]] std::int32_t skyFor(float temperature) {
    const float scaled = std::clamp(temperature / 3.0F, -1.0F, 1.0F);
    return hsvToRgb(0.62222224F - scaled * 0.05F, 0.5F + scaled * 0.1F, 1.0F);
}

[[nodiscard]] std::optional<nbt::NbtTag> oldEffects(const std::optional<nbt::NbtTag>& value) {
    if (!value.has_value()) {
        return value;
    }
    const nbt::NbtCompound* biome = value->get<nbt::NbtCompound>();
    if (biome == nullptr) {
        return value;
    }
    const nbt::NbtCompound* attributes = biome->getCompound("attributes");
    const nbt::NbtCompound* effects = biome->getCompound("effects");

    nbt::NbtCompound rewritten;
    for (std::size_t i = 0; i < biome->size(); ++i) {
        const std::string& name = biome->names()[i];
        if (name != "attributes" && name != "effects") {
            rewritten.set(name, biome->values()[i]);
        }
    }

    nbt::NbtCompound old;
    if (effects != nullptr) {
        for (std::size_t i = 0; i < effects->size(); ++i) {
            const std::string& name = effects->names()[i];
            if (const std::optional<std::int32_t> colour = colourOf(&effects->values()[i])) {
                old.putInt(name, *colour);
            } else {
                old.set(name, effects->values()[i]);
            }
        }
    }
    old.putInt(kFog,
               colourFrom(attributes, kVisualFog).or_else([&] { return intOf(old, kFog); }).value_or(kDefaultFog));
    old.putInt(kWater, intOf(old, kWater).value_or(kDefaultWater));
    old.putInt(kWaterFog, colourFrom(attributes, kVisualWaterFog)
                              .or_else([&] { return intOf(old, kWaterFog); })
                              .value_or(kDefaultWaterFog));
    old.putInt(kSky, colourFrom(attributes, kVisualSky)
                         .or_else([&] { return intOf(old, kSky); })
                         .value_or(skyFor(biome->getFloat(kTemperature, 0.8F))));
    if (attributes != nullptr) {
        if (const nbt::NbtCompound* ambient = attributes->getCompound(kAudioAmbient)) {
            if (const nbt::NbtTag* mood = ambient->find("mood")) {
                old.set("mood_sound", *mood);
            }
        }
        if (const nbt::NbtCompound* music = attributes->getCompound(kAudioMusic)) {
            if (const nbt::NbtTag* fallback = music->find("default")) {
                old.set("music", *fallback);
            }
        }
    }
    rewritten.set("effects", nbt::NbtTag::ofCompound(std::move(old)));
    return nbt::NbtTag::ofCompound(std::move(rewritten));
}

} // namespace

bool Link773::clientbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    if (state == ProtocolState::Configuration &&
        packet.nativeId() == protocol::configuration::RegistryDataPacket::kId) {
        rewriteRegistry(packet);
        return true;
    }
    if (state == ProtocolState::Play && packet.nativeId() == play::SetBorderLerpSizePacket::kId) {
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            auto border = play::SetBorderLerpSizePacket::read(input);
            if (!border.has_value()) {
                return;
            }
            constexpr std::int64_t kLimit = std::numeric_limits<std::int64_t>::max() / kMillisPerTick;
            border->lerpTimeTicks = std::clamp<std::int64_t>(border->lerpTimeTicks, 0, kLimit) * kMillisPerTick;
            border->write(output);
        });
    }
    return true;
}

void Link773::rewriteRegistry(MutablePacket& packet) const {
    auto data =
        decode(packet, [](io::ByteBuffer& in) { return protocol::configuration::RegistryDataPacket::read(in); });
    if (!data.has_value()) {
        return;
    }
    if (data->registryId == kDimensionType) {
        for (protocol::configuration::RegistryEntry& entry : data->entries) {
            entry.value = ownDimension(entry.key);
        }
    } else if (data->registryId == kBiome) {
        for (protocol::configuration::RegistryEntry& entry : data->entries) {
            entry.value = oldEffects(entry.value);
        }
    } else {
        return;
    }
    encode(packet, [&](io::ByteBuffer& out) { data->write(out); });
}

std::optional<nbt::NbtTag> Link773::ownDimension(const std::string& name) const {
    auto found = tables().dimensionTypes.find(name);
    if (found == tables().dimensionTypes.end()) {
        found = tables().dimensionTypes.find(std::string{kOverworld});
    }
    if (found == tables().dimensionTypes.end()) {
        return std::nullopt;
    }
    std::expected<nbt::NbtTag, nbt::NbtError> parsed = nbt::snbt::parse(found->second);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    return *std::move(parsed);
}

} // namespace basalt::wire::multiversion::java
