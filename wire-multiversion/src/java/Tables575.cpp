// Regenerate rather than edit: every number here is a protocol fact, not a decision.

#include <basalt/wire/multiversion/java/Tables575.hpp>

namespace basalt::wire::multiversion::java {

const VersionTables& tables575() {
    static const VersionTables tables = [] {
        VersionTables built;
        built.older = 575;
        built.blockStates = IdMap{"0:11337:0|"};
        built.mobEffect = IdMap{"1:32:0|"};
        built.staticBiomes = NameIdMap{
            "minecraft:badlands:37;minecraft:badlands_plateau:39;minecraft:bamboo_jungle:168;minecraft:bamboo_jungle_"
            "hills:169;minecraft:beach:16;minecraft:birch_forest:27;minecraft:birch_forest_hills:28;minecraft:cold_"
            "ocean:46;minecraft:dark_forest:29;minecraft:dark_forest_hills:157;minecraft:deep_cold_ocean:49;minecraft:"
            "deep_frozen_ocean:50;minecraft:deep_lukewarm_ocean:48;minecraft:deep_ocean:24;minecraft:deep_warm_ocean:"
            "47;minecraft:desert:2;minecraft:desert_hills:17;minecraft:desert_lakes:130;minecraft:end_barrens:43;"
            "minecraft:end_highlands:42;minecraft:end_midlands:41;minecraft:eroded_badlands:165;minecraft:flower_"
            "forest:132;minecraft:forest:4;minecraft:frozen_ocean:10;minecraft:frozen_river:11;minecraft:giant_spruce_"
            "taiga:160;minecraft:giant_spruce_taiga_hills:161;minecraft:giant_tree_taiga:32;minecraft:giant_tree_taiga_"
            "hills:33;minecraft:gravelly_mountains:131;minecraft:ice_spikes:140;minecraft:jungle:21;minecraft:jungle_"
            "edge:23;minecraft:jungle_hills:22;minecraft:lukewarm_ocean:45;minecraft:modified_badlands_plateau:167;"
            "minecraft:modified_gravelly_mountains:162;minecraft:modified_jungle:149;minecraft:modified_jungle_edge:"
            "151;minecraft:modified_wooded_badlands_plateau:166;minecraft:mountain_edge:20;minecraft:mountains:3;"
            "minecraft:mushroom_field_shore:15;minecraft:mushroom_fields:14;minecraft:nether:8;minecraft:ocean:0;"
            "minecraft:plains:1;minecraft:river:7;minecraft:savanna:35;minecraft:savanna_plateau:36;minecraft:"
            "shattered_savanna:163;minecraft:shattered_savanna_plateau:164;minecraft:small_end_islands:40;minecraft:"
            "snowy_beach:26;minecraft:snowy_mountains:13;minecraft:snowy_taiga:30;minecraft:snowy_taiga_hills:31;"
            "minecraft:snowy_taiga_mountains:158;minecraft:snowy_tundra:12;minecraft:stone_shore:25;minecraft:"
            "sunflower_plains:129;minecraft:swamp:6;minecraft:swamp_hills:134;minecraft:taiga:5;minecraft:taiga_hills:"
            "19;minecraft:taiga_mountains:133;minecraft:tall_birch_forest:155;minecraft:tall_birch_hills:156;minecraft:"
            "the_end:9;minecraft:the_void:127;minecraft:warm_ocean:44;minecraft:wooded_badlands_plateau:38;minecraft:"
            "wooded_hills:18;minecraft:wooded_mountains:34"};
        built.playerEntityType = 101;
        built.newerPlayerEntityType = 101;
        built.blockStateDirectBits = 14;
        return built;
    }();
    return tables;
}

} // namespace basalt::wire::multiversion::java
