#include <basalt/wire/bedrock/BedrockMapConversion.hpp>

#include <array>
#include <utility>

namespace basalt::wire::bedrock {
namespace {

struct Icon {
    std::uint8_t image = 0;
    std::uint32_t color = 0xFFFFFFFFU;
};

// parity: `MapColor`, by base colour id.
constexpr std::array<std::uint32_t, 64> kMapColors{{
    0,        8368696, 16247203, 13092807, 16711680, 10526975, 10987431, 31744,    16777215, 10791096, 9923917,
    7368816,  4210943, 9402184,  16776437, 14188339, 11685080, 6724056,  15066419, 8375321,  15892389, 5000268,
    10066329, 5013401, 8339378,  3361970,  6704179,  6717235,  10040115, 1644825,  16445005, 6085589,  4882687,
    55610,    8476209, 7340544,  13742497, 10441252, 9787244,  7367818,  12223780, 6780213,  10505550, 3746083,
    8874850,  5725276, 8014168,  4996700,  4993571,  5001770,  9321518,  2430480,  12398641, 9715553,  6035741,
    1474182,  3837580, 5647422,  1356933,  6579300,  14200723, 8365974,  0,        0,
}};

// parity: `MapColor.Brightness.modifier`, by brightness id.
constexpr std::array<std::uint32_t, 4> kModifiers{180, 220, 255, 135};

constexpr std::array<Icon, 40> kIcons{{
    {0},
    {7},
    {2},
    {3},
    {4, 0xFF000000U},
    {5},
    {6},
    {13},
    {14},
    {15},
    {13, 0xFFFFFFFFU},
    {13, 0xFFF9801DU},
    {13, 0xFFC74EBDU},
    {13, 0xFF3AB3DAU},
    {13, 0xFFFED83DU},
    {13, 0xFF80C71FU},
    {13, 0xFFF38BAAU},
    {13, 0xFF474F52U},
    {13, 0xFF9D9D97U},
    {13, 0xFF169C9CU},
    {13, 0xFF8932B8U},
    {13, 0xFF3C44AAU},
    {13, 0xFF835432U},
    {13, 0xFF5E7C16U},
    {13, 0xFFB02E26U},
    {13, 0xFF1D1D21U},
    {4},
    {17},
    {18},
    {19},
    {20},
    {21},
    {22},
    {23},
    {24},
    {4},
    {4},
    {4},
    {4},
    {4},
}};

constexpr std::array<std::pair<std::string_view, std::int32_t>, 16> kExplorerData{{
    {"minecraft:ocean_monument_map", 3},
    {"minecraft:woodland_mansion_map", 4},
    {"minecraft:buried_trial_chambers_map", 14},
    {"minecraft:jungle_pyramid_map", 12},
    {"minecraft:swamp_hut_map", 13},
    {"minecraft:desert_village_map", 11},
    {"minecraft:plains_village_map", 9},
    {"minecraft:savanna_village_map", 10},
    {"minecraft:snowy_village_map", 7},
    {"minecraft:taiga_village_map", 8},
    {"minecraft:buried_treasure_map", 5},
    {"minecraft:buried_ancient_city_map", 23},
    {"minecraft:buried_mineshaft_map", 24},
    {"minecraft:desert_pyramid_map", 25},
    {"minecraft:warm_ocean_ruins_map", 26},
    {"minecraft:abandoned_camp_map", 18},
}};

[[nodiscard]] std::uint32_t abgrOf(const std::uint8_t packed) {
    const std::size_t id = packed >> 2U;
    if (id == 0 || id >= kMapColors.size()) {
        return 0;
    }
    const std::uint32_t rgb = kMapColors[id];
    const std::uint32_t modifier = kModifiers[packed & 3U];
    const std::uint32_t red = ((rgb >> 16U) & 0xFFU) * modifier / 255U;
    const std::uint32_t green = ((rgb >> 8U) & 0xFFU) * modifier / 255U;
    const std::uint32_t blue = (rgb & 0xFFU) * modifier / 255U;
    return 0xFF000000U | (blue << 16U) | (green << 8U) | red;
}

} // namespace

BedrockMapData bedrockMapOf(const protocol::play::MapItemDataPacket& update, const std::uint8_t dimension) {
    BedrockMapData map{.mapId = update.mapId,
                       .dimension = dimension,
                       .locked = update.locked,
                       .scale = update.scale,
                       .created = {update.mapId}};
    if (update.decorations.has_value()) {
        std::vector<BedrockMapDecoration> decorations;
        for (const protocol::play::MapDecorationEntry& entry : *update.decorations) {
            const Icon icon = entry.type >= 0 && static_cast<std::size_t>(entry.type) < kIcons.size()
                                  ? kIcons[static_cast<std::size_t>(entry.type)]
                                  : Icon{4};
            decorations.push_back({.image = icon.image,
                                   .rotation = static_cast<std::uint8_t>(entry.rotation),
                                   .x = entry.x,
                                   .y = entry.y,
                                   .color = icon.color});
        }
        map.decorations = std::move(decorations);
    }
    if (update.patch.has_value() && update.patch->width > 0) {
        const protocol::play::MapColorPatch& patch = *update.patch;
        BedrockMapTexture texture{.width = patch.width, .height = patch.height, .x = patch.startX, .y = patch.startY};
        texture.colors.reserve(patch.colors.size());
        for (std::int32_t y = 0; y < patch.height; ++y) {
            for (std::int32_t x = 0; x < patch.width; ++x) {
                const auto at = static_cast<std::size_t>(x + (y * patch.width));
                texture.colors.push_back(at < patch.colors.size() ? abgrOf(patch.colors[at]) : 0U);
            }
        }
        map.texture = std::move(texture);
    }
    return map;
}

std::int32_t bedrockMapExplorerData(const std::string_view javaItem) {
    for (const auto& [java, data] : kExplorerData) {
        if (java == javaItem) {
            return data;
        }
    }
    return 0;
}

} // namespace basalt::wire::bedrock
