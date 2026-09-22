// The two editions name a banner's patterns differently: this one spells each out and that one
// abbreviates it. The list is the 26.2 banner_pattern registry, in its own alphabetical order.

#include <basalt/wire/bedrock/BedrockBanners.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace basalt::wire::bedrock {
namespace {

using Pair = std::pair<std::string_view, std::string_view>;

constexpr std::array<Pair, 43> kPatterns{{
    {"base", "b"},
    {"border", "bo"},
    {"bricks", "bri"},
    {"circle", "mc"},
    {"creeper", "cre"},
    {"cross", "cr"},
    {"curly_border", "cbo"},
    {"diagonal_left", "ld"},
    {"diagonal_right", "rud"},
    {"diagonal_up_left", "lud"},
    {"diagonal_up_right", "rd"},
    {"flow", "flw"},
    {"flower", "flo"},
    {"globe", "glb"},
    {"gradient", "gra"},
    {"gradient_up", "gru"},
    {"guster", "gus"},
    {"half_horizontal", "hh"},
    {"half_horizontal_bottom", "hhb"},
    {"half_vertical", "vh"},
    {"half_vertical_right", "vhr"},
    {"mojang", "moj"},
    {"piglin", "pig"},
    {"rhombus", "mr"},
    {"skull", "sku"},
    {"small_stripes", "ss"},
    {"square_bottom_left", "bl"},
    {"square_bottom_right", "br"},
    {"square_top_left", "tl"},
    {"square_top_right", "tr"},
    {"straight_cross", "sc"},
    {"stripe_bottom", "bs"},
    {"stripe_center", "cs"},
    {"stripe_downleft", "dls"},
    {"stripe_downright", "drs"},
    {"stripe_left", "ls"},
    {"stripe_middle", "ms"},
    {"stripe_right", "rs"},
    {"stripe_top", "ts"},
    {"triangle_bottom", "bt"},
    {"triangle_top", "tt"},
    {"triangles_bottom", "bts"},
    {"triangles_top", "tts"},
}};

/// The sixteen dyes in the order both editions number them, which is the order a banner's colour
/// is saved under as a name.
constexpr std::array<std::string_view, 16> kDyes{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                 "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                 "brown", "green",  "red",        "black"};

} // namespace

std::string_view bedrockBannerPattern(std::string_view javaPattern) {
    if (javaPattern.starts_with("minecraft:")) {
        javaPattern.remove_prefix(std::string_view{"minecraft:"}.size());
    }
    const auto found = std::ranges::lower_bound(kPatterns, javaPattern, {}, &Pair::first);
    // parity: Geyser falls back to the plain field rather than dropping a layer it cannot name.
    return found == kPatterns.end() || found->first != javaPattern ? kPatterns.front().second : found->second;
}

std::int32_t dyeIndex(std::string_view name) {
    if (name.starts_with("minecraft:")) {
        name.remove_prefix(std::string_view{"minecraft:"}.size());
    }
    const auto found = std::ranges::find(kDyes, name);
    return found == kDyes.end() ? 0 : static_cast<std::int32_t>(found - kDyes.begin());
}

} // namespace basalt::wire::bedrock
