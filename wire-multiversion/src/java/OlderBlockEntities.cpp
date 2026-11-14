#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/BlockEntityTypes.hpp>
#include <basalt/wire/multiversion/java/OlderBlockEntities.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::wire::multiversion::java::olderBlockEntities {
namespace {

namespace versions = protocolVersions;

constexpr std::string_view kNamespace = "minecraft:";

// Native block states, by 26.3 registration order: each head kind's 32 standing states, then its 8 on a wall.
constexpr std::int32_t kFirstSkullState = 12682;
constexpr std::int32_t kSkullStatesPerKind = 40;
constexpr std::int32_t kStandingSkullStates = 32;
constexpr std::int32_t kRotations = 16;
// parity: TileEntitySkull's SkullType for skeleton, wither, zombie, player, creeper and dragon; a piglin had none.
constexpr std::array<std::int8_t, 7> kSkullTypes = {0, 1, 2, 3, 4, 5, 2};
constexpr std::int32_t kFirstBannerState = 14774;
constexpr std::int32_t kFirstWallBannerState = 15030;
constexpr std::int32_t kWallBannerStates = 4;
constexpr std::int32_t kDyeCount = 16;

[[nodiscard]] bool isSkull(std::int32_t state) {
    return state >= kFirstSkullState &&
           state < kFirstSkullState + kSkullStatesPerKind * static_cast<std::int32_t>(kSkullTypes.size());
}

[[nodiscard]] std::optional<std::int32_t> bannerColour(std::int32_t state) {
    if (state >= kFirstBannerState && state < kFirstWallBannerState) {
        return (state - kFirstBannerState) / kRotations;
    }
    if (state >= kFirstWallBannerState && state < kFirstWallBannerState + kWallBannerStates * kDyeCount) {
        return (state - kFirstWallBannerState) / kWallBannerStates;
    }
    return std::nullopt;
}

// parity: DyeColor, in id order.
constexpr std::array<std::string_view, 16> kDyes = {
    "white",      "orange", "magenta", "light_blue", "yellow", "lime",  "pink", "gray",
    "light_gray", "cyan",   "purple",  "blue",       "brown",  "green", "red",  "black",
};

// parity: BannerPatterns, the short code each pattern was saved by before 1.20.5.
constexpr std::array<std::pair<std::string_view, std::string_view>, 43> kPatterns = {{
    {"base", "b"},
    {"square_bottom_left", "bl"},
    {"square_bottom_right", "br"},
    {"square_top_left", "tl"},
    {"square_top_right", "tr"},
    {"stripe_bottom", "bs"},
    {"stripe_top", "ts"},
    {"stripe_left", "ls"},
    {"stripe_right", "rs"},
    {"stripe_center", "cs"},
    {"stripe_middle", "ms"},
    {"stripe_downright", "drs"},
    {"stripe_downleft", "dls"},
    {"small_stripes", "ss"},
    {"cross", "cr"},
    {"straight_cross", "sc"},
    {"triangle_bottom", "bt"},
    {"triangle_top", "tt"},
    {"triangles_bottom", "bts"},
    {"triangles_top", "tts"},
    {"diagonal_left", "ld"},
    {"diagonal_up_right", "rd"},
    {"diagonal_up_left", "lud"},
    {"diagonal_right", "rud"},
    {"circle", "mc"},
    {"rhombus", "mr"},
    {"half_vertical", "vh"},
    {"half_horizontal", "hh"},
    {"half_vertical_right", "vhr"},
    {"half_horizontal_bottom", "hhb"},
    {"border", "bo"},
    {"curly_border", "cbo"},
    {"gradient", "gra"},
    {"gradient_up", "gru"},
    {"bricks", "bri"},
    {"globe", "glb"},
    {"creeper", "cre"},
    {"skull", "sku"},
    {"flower", "flo"},
    {"mojang", "moj"},
    {"piglin", "pig"},
    {"flow", "flw"},
    {"guster", "gus"},
}};

// parity: EntityList before 1.11, the names a spawner went by; later variants fall back to the mob they split from.
constexpr std::array<std::pair<std::string_view, std::string_view>, 46> kEntityNamesBefore1_11 = {{
    {"creeper", "Creeper"},
    {"skeleton", "Skeleton"},
    {"wither_skeleton", "Skeleton"},
    {"stray", "Skeleton"},
    {"spider", "Spider"},
    {"giant", "Giant"},
    {"zombie", "Zombie"},
    {"husk", "Zombie"},
    {"zombie_villager", "Zombie"},
    {"drowned", "Zombie"},
    {"slime", "Slime"},
    {"ghast", "Ghast"},
    {"zombified_piglin", "PigZombie"},
    {"enderman", "Enderman"},
    {"cave_spider", "CaveSpider"},
    {"silverfish", "Silverfish"},
    {"blaze", "Blaze"},
    {"magma_cube", "LavaSlime"},
    {"ender_dragon", "EnderDragon"},
    {"wither", "WitherBoss"},
    {"bat", "Bat"},
    {"witch", "Witch"},
    {"endermite", "Endermite"},
    {"guardian", "Guardian"},
    {"elder_guardian", "Guardian"},
    {"shulker", "Shulker"},
    {"pig", "Pig"},
    {"sheep", "Sheep"},
    {"cow", "Cow"},
    {"chicken", "Chicken"},
    {"squid", "Squid"},
    {"wolf", "Wolf"},
    {"mooshroom", "MushroomCow"},
    {"snow_golem", "SnowMan"},
    {"ocelot", "Ozelot"},
    {"iron_golem", "VillagerGolem"},
    {"horse", "EntityHorse"},
    {"donkey", "EntityHorse"},
    {"mule", "EntityHorse"},
    {"skeleton_horse", "EntityHorse"},
    {"zombie_horse", "EntityHorse"},
    {"rabbit", "Rabbit"},
    {"polar_bear", "PolarBear"},
    {"villager", "Villager"},
    {"armor_stand", "ArmorStand"},
    {"item", "Item"},
}};

std::string_view bare(std::string_view key) {
    return key.starts_with(kNamespace) ? key.substr(kNamespace.size()) : key;
}

std::string uuidText(std::span<const std::int32_t> words) {
    std::array<std::uint32_t, 4> parts{};
    for (std::size_t index = 0; index < parts.size() && index < words.size(); ++index) {
        parts[index] = static_cast<std::uint32_t>(words[index]);
    }
    std::array<char, 37> text{};
    std::snprintf(text.data(), text.size(), "%08x-%04x-%04x-%04x-%04x%08x", parts[0], parts[1] >> 16, parts[1] & 0xFFFF,
                  parts[2] >> 16, parts[2] & 0xFFFF, parts[3]);
    return std::string{text.data()};
}

void skullState(nbt::NbtCompound& tag, std::int32_t state) {
    const std::int32_t kind = (state - kFirstSkullState) / kSkullStatesPerKind;
    const std::int32_t offset = (state - kFirstSkullState) % kSkullStatesPerKind;
    tag.putByte("SkullType", kSkullTypes[static_cast<std::size_t>(kind)]);
    tag.putByte("Rot", static_cast<std::int8_t>(offset < kStandingSkullStates ? offset % kRotations : 0));
}

/// parity: ResolvableProfile, saved as the SkullOwner a GameProfile was before 1.20.5.
void skull(nbt::NbtCompound& tag, std::int32_t farEnd, std::optional<std::int32_t> state) {
    const bool placed = farEnd < versions::kV1_13 && state.has_value() && isSkull(*state);
    if (placed) {
        skullState(tag, *state);
    }
    const nbt::NbtTag* profile = tag.find("profile");
    if (profile == nullptr) {
        return;
    }
    nbt::NbtCompound owner;
    if (const auto* name = profile->get<std::string>()) {
        owner.putString("Name", *name);
    } else if (const auto* full = profile->get<nbt::NbtCompound>()) {
        if (full->contains("name")) {
            owner.putString("Name", std::string{full->getString("name")});
        }
        if (const std::span<const std::int32_t> id = full->getIntArray("id"); id.size() == 4) {
            if (farEnd >= versions::kV1_16) {
                owner.putIntArray("Id", {id.begin(), id.end()});
            } else {
                owner.putString("Id", uuidText(id));
            }
        }
        nbt::NbtList textures{nbt::NbtType::Compound};
        if (const nbt::NbtList* properties = full->getList("properties")) {
            for (const nbt::NbtTag& entry : *properties) {
                const auto* property = entry.get<nbt::NbtCompound>();
                if (property == nullptr || property->getString("name") != "textures") {
                    continue;
                }
                nbt::NbtCompound texture;
                texture.putString("Value", std::string{property->getString("value")});
                if (property->contains("signature")) {
                    texture.putString("Signature", std::string{property->getString("signature")});
                }
                textures.push(nbt::NbtTag::ofCompound(std::move(texture)));
            }
        }
        if (!textures.empty()) {
            nbt::NbtCompound properties;
            properties.set("textures", nbt::NbtTag::ofList(std::move(textures)));
            owner.set("Properties", nbt::NbtTag::ofCompound(std::move(properties)));
        }
    }
    tag.remove("profile");
    if (farEnd < versions::kV1_13 && !placed) {
        // parity: TileEntitySkull, whose type was its own byte before the flattening; a profile means a player.
        tag.putByte("SkullType", 3);
    }
    tag.set(farEnd < versions::kV1_16 ? "Owner" : "SkullOwner", nbt::NbtTag::ofCompound(std::move(owner)));
}

void banner(nbt::NbtCompound& tag, std::int32_t farEnd, std::optional<std::int32_t> state) {
    if (const std::optional<std::int32_t> colour = state.has_value() ? bannerColour(*state) : std::nullopt;
        colour.has_value() && farEnd < versions::kV1_13) {
        // parity: TileEntityBanner's Base, the dye damage of the banner's own colour.
        tag.putInt("Base", kDyeCount - 1 - *colour);
    }
    const nbt::NbtList* patterns = tag.getList("patterns");
    if (patterns == nullptr) {
        return;
    }
    nbt::NbtList older{nbt::NbtType::Compound};
    for (const nbt::NbtTag& entry : *patterns) {
        const auto* layer = entry.get<nbt::NbtCompound>();
        const nbt::NbtTag* key = layer == nullptr ? nullptr : layer->find("pattern");
        const std::string* pattern = key == nullptr ? nullptr : key->get<std::string>();
        if (pattern == nullptr) {
            continue;
        }
        const auto code =
            std::ranges::find(kPatterns, bare(*pattern), &std::pair<std::string_view, std::string_view>::first);
        const auto dye = std::ranges::find(kDyes, layer->getString("color", "white"));
        if (code == kPatterns.end() || dye == kDyes.end()) {
            continue;
        }
        const auto color = static_cast<std::int32_t>(dye - kDyes.begin());
        nbt::NbtCompound layered;
        layered.putString("Pattern", std::string{code->second});
        // parity: before 1.13 a layer was coloured by its dye's damage, the colour id read backwards.
        layered.putInt("Color", farEnd < versions::kV1_13 ? 15 - color : color);
        older.push(nbt::NbtTag::ofCompound(std::move(layered)));
    }
    tag.remove("patterns");
    tag.set("Patterns", nbt::NbtTag::ofList(std::move(older)));
}

/// parity: SpawnData, which held the entity itself before 1.18 wrapped it.
void spawner(nbt::NbtCompound& tag, std::int32_t farEnd) {
    const nbt::NbtCompound* data = tag.getCompound("SpawnData");
    const nbt::NbtCompound* entity = data == nullptr ? nullptr : data->getCompound("entity");
    if (entity == nullptr || farEnd >= versions::kV1_18) {
        return;
    }
    nbt::NbtCompound inner = *entity;
    if (farEnd < versions::kV1_11) {
        const std::string_view id = bare(inner.getString("id"));
        const auto legacy =
            std::ranges::find(kEntityNamesBefore1_11, id, &std::pair<std::string_view, std::string_view>::first);
        const std::string name{legacy == kEntityNamesBefore1_11.end() ? std::string_view{"Pig"} : legacy->second};
        inner.putString("id", name);
        if (farEnd < versions::kV1_9) {
            tag.putString("EntityId", name);
        }
    }
    tag.set("SpawnData", nbt::NbtTag::ofCompound(std::move(inner)));
}

} // namespace

bool shapedByState(std::int32_t nativeState) { return isSkull(nativeState) || bannerColour(nativeState).has_value(); }

bool readsState(std::int32_t nativeType, std::int32_t farEnd) {
    const std::string_view name = blockEntityTypes::nativeName(nativeType);
    return farEnd < versions::kV1_13 && (name == "minecraft:skull" || name == "minecraft:banner");
}

void rewrite(std::int32_t nativeType, nbt::NbtCompound& tag, std::int32_t farEnd, std::optional<std::int32_t> state) {
    const std::string_view name = blockEntityTypes::nativeName(nativeType);
    if (name == "minecraft:skull") {
        skull(tag, farEnd, state);
    } else if (name == "minecraft:banner") {
        banner(tag, farEnd, state);
    } else if (name == "minecraft:mob_spawner") {
        spawner(tag, farEnd);
    }
}

void rewrite(std::int32_t nativeType, std::optional<nbt::NbtCompound>& tag, std::int32_t farEnd,
             std::optional<std::int32_t> nativeState) {
    if (!tag.has_value() && nativeState.has_value() && shapedByState(*nativeState)) {
        tag.emplace();
    }
    if (tag.has_value()) {
        rewrite(nativeType, *tag, farEnd, nativeState);
    }
}

} // namespace basalt::wire::multiversion::java::olderBlockEntities
