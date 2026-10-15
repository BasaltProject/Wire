// Regenerate rather than edit: every number here is a protocol fact, not a decision.

#include <basalt/wire/multiversion/java/Tables751.hpp>

namespace basalt::wire::multiversion::java {

const VersionTables& tables751() {
    static const VersionTables tables = [] {
        VersionTables built;
        built.older = 751;
        built.blockStates = IdMap{"0:17112:0|"};
        built.mobEffect = IdMap{"1:32:0|"};
        built.attributeNames =
            NameIdMap{"minecraft:generic.armor:8;minecraft:generic.armor_toughness:9;minecraft:generic.attack_damage:5;"
                      "minecraft:generic.attack_knockback:6;minecraft:generic.attack_speed:7;minecraft:generic.flying_"
                      "speed:4;minecraft:generic.follow_range:1;minecraft:generic.knockback_resistance:2;minecraft:"
                      "generic.luck:10;minecraft:generic.max_health:0;minecraft:generic.movement_speed:3;minecraft:"
                      "horse.jump_strength:12;minecraft:zombie.spawn_reinforcements:11"};
        built.playerEntityType = 106;
        built.newerPlayerEntityType = 106;
        built.blockStateDirectBits = 15;
        return built;
    }();
    return tables;
}

} // namespace basalt::wire::multiversion::java
