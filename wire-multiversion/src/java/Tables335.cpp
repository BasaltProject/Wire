// Regenerate rather than edit: every number here is a protocol fact, not a decision.

#include <basalt/wire/multiversion/java/Tables335.hpp>

namespace basalt::wire::multiversion::java {

const VersionTables& tables335() {
    static const VersionTables tables = [] {
        VersionTables built;
        built.older = 335;
        std::map<protocol::ProtocolState, PacketIdMap> built_clientbound;
        std::map<protocol::ProtocolState, PacketIdMap> built_serverbound;
        built_serverbound.emplace(
            protocol::ProtocolState::Play,
            PacketIdMap{"0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,-1,19,20,21,22,23,24,25,26,27,28,29,30,31,32"});
        built_clientbound.emplace(
            protocol::ProtocolState::Play,
            PacketIdMap{"0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,"
                        "34,35,36,37,38,39,40,41,42,-1,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,"
                        "64,65,66,67,68,69,70,71,72,73,74,75,76,77,78"});
        built.packets = PacketIdTables{std::move(built_clientbound), std::move(built_serverbound)};
        built.blockStates = IdMap{"0:4084:0|"};
        built.item = IdMap{"1:25:0;27:5:0;33:1:0;35:1:0;37:18:0;56:3:0;60:3:0;65:3:0;69:2:0;72:2:0;76:7:0;84:8:0;95:9:"
                           "0;106:9:0;116:1:0;119:5:0;125:7:0;133:7:0;141:3:0;145:4:0;151:25:0;179:2:0;182:11:0;198:6:"
                           "0;205:2:0;208:1:0;210:2:0;213:40:0;255:196:0;452:2:0;2256:12:0|"};
        built.entityType = IdMap{
            "1:6:0;9:2:0;16:2:0;23:1:0;27:3:0;31:2:0;34:4:0;40:1:0;43:5:0;50:24:0;75:5:0;90:14:0;105:1:0;120:1:0|"};
        built.mobEffect = IdMap{"1:27:0|"};
        built.staticBiomes = NameIdMap{
            "minecraft:badlands:37;minecraft:badlands_plateau:39;minecraft:beach:16;minecraft:birch_forest:27;"
            "minecraft:birch_forest_hills:28;minecraft:dark_forest:29;minecraft:dark_forest_hills:157;minecraft:deep_"
            "ocean:24;minecraft:desert:2;minecraft:desert_hills:17;minecraft:desert_lakes:130;minecraft:eroded_"
            "badlands:165;minecraft:flower_forest:132;minecraft:forest:4;minecraft:frozen_ocean:10;minecraft:frozen_"
            "river:11;minecraft:giant_spruce_taiga:160;minecraft:giant_spruce_taiga_hills:161;minecraft:giant_tree_"
            "taiga:32;minecraft:giant_tree_taiga_hills:33;minecraft:gravelly_mountains:131;minecraft:ice_spikes:140;"
            "minecraft:jungle:21;minecraft:jungle_edge:23;minecraft:jungle_hills:22;minecraft:modified_badlands_"
            "plateau:167;minecraft:modified_gravelly_mountains:162;minecraft:modified_jungle:149;minecraft:modified_"
            "jungle_edge:151;minecraft:modified_wooded_badlands_plateau:166;minecraft:mountain_edge:20;minecraft:"
            "mountains:3;minecraft:mushroom_field_shore:15;minecraft:mushroom_fields:14;minecraft:nether_wastes:8;"
            "minecraft:ocean:0;minecraft:plains:1;minecraft:river:7;minecraft:savanna:35;minecraft:savanna_plateau:36;"
            "minecraft:shattered_savanna:163;minecraft:shattered_savanna_plateau:164;minecraft:snowy_beach:26;"
            "minecraft:snowy_mountains:13;minecraft:snowy_taiga:30;minecraft:snowy_taiga_hills:31;minecraft:snowy_"
            "taiga_mountains:158;minecraft:snowy_tundra:12;minecraft:stone_shore:25;minecraft:sunflower_plains:129;"
            "minecraft:swamp:6;minecraft:swamp_hills:134;minecraft:taiga:5;minecraft:taiga_hills:19;minecraft:taiga_"
            "mountains:133;minecraft:tall_birch_forest:155;minecraft:tall_birch_hills:156;minecraft:the_end:9;"
            "minecraft:the_void:127;minecraft:wooded_badlands_plateau:38;minecraft:wooded_hills:18;minecraft:wooded_"
            "mountains:34"};
        built.blockStateDirectBits = 13;
        return built;
    }();
    return tables;
}

} // namespace basalt::wire::multiversion::java
