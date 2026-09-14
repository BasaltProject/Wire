#include <basalt/wire/bedrock/BedrockEntityIdentifiers.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace basalt::wire::bedrock {

std::string_view bedrockEntityName(const std::string_view javaType) {
    // The fourteen the two editions spell differently. Everything else is the same word on both,
    // and there is nothing in a name that says which kind it is, so the list is the whole answer.
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 14> kRenamed{{
        {"minecraft:breeze_wind_charge", "minecraft:breeze_wind_charge_projectile"},
        {"minecraft:end_crystal", "minecraft:ender_crystal"},
        {"minecraft:evoker_fangs", "minecraft:evocation_fang"},
        {"minecraft:experience_bottle", "minecraft:xp_bottle"},
        {"minecraft:experience_orb", "minecraft:xp_orb"},
        {"minecraft:eye_of_ender", "minecraft:eye_of_ender_signal"},
        {"minecraft:firework_rocket", "minecraft:fireworks_rocket"},
        {"minecraft:fishing_bobber", "minecraft:fishing_hook"},
        {"minecraft:trident", "minecraft:thrown_trident"},
        {"minecraft:tropical_fish", "minecraft:tropicalfish"},
        {"minecraft:villager", "minecraft:villager_v2"},
        {"minecraft:wind_charge", "minecraft:wind_charge_projectile"},
        {"minecraft:zombie_villager", "minecraft:zombie_villager_v2"},
        {"minecraft:zombified_piglin", "minecraft:zombie_pigman"},
    }};

    const auto found = std::ranges::find_if(kRenamed, [javaType](const auto& row) { return row.first == javaType; });
    return found == kRenamed.end() ? javaType : found->second;
}

} // namespace basalt::wire::bedrock
