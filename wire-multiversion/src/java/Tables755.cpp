// Regenerate rather than edit: every number here is a protocol fact, not a decision.

#include <basalt/wire/multiversion/java/Tables755.hpp>

namespace basalt::wire::multiversion::java {

const VersionTables& tables755() {
    static const VersionTables tables = [] {
        VersionTables built;
        built.older = 755;
        std::map<protocol::ProtocolState, PacketIdMap> built_clientbound;
        std::map<protocol::ProtocolState, PacketIdMap> built_serverbound;
        built_clientbound.emplace(
            protocol::ProtocolState::Play,
            PacketIdMap{
                "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,"
                "37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,-1,59,60,61,62,63,64,65,66,67,68,69,70,"
                "71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102"});
        built.packets = PacketIdTables{std::move(built_clientbound), std::move(built_serverbound)};
        built.blockStates = IdMap{"0:20342:0|"};
        built.mobEffect = IdMap{"1:32:0|"};
        built.attributeNames =
            NameIdMap{"minecraft:generic.armor:8;minecraft:generic.armor_toughness:9;minecraft:generic.attack_damage:5;"
                      "minecraft:generic.attack_knockback:6;minecraft:generic.attack_speed:7;minecraft:generic.flying_"
                      "speed:4;minecraft:generic.follow_range:1;minecraft:generic.knockback_resistance:2;minecraft:"
                      "generic.luck:10;minecraft:generic.max_health:0;minecraft:generic.movement_speed:3;minecraft:"
                      "horse.jump_strength:12;minecraft:zombie.spawn_reinforcements:11"};
        built.playerEntityType = 111;
        built.newerPlayerEntityType = 111;
        built.blockStateDirectBits = 15;
        return built;
    }();
    return tables;
}

} // namespace basalt::wire::multiversion::java
