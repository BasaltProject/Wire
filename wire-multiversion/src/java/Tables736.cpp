// Regenerate rather than edit: every number here is a protocol fact, not a decision.

#include <basalt/wire/multiversion/java/Tables736.hpp>

namespace basalt::wire::multiversion::java {

const VersionTables& tables736() {
    static const VersionTables tables = [] {
        VersionTables built;
        built.older = 736;
        std::map<protocol::ProtocolState, PacketIdMap> built_clientbound;
        std::map<protocol::ProtocolState, PacketIdMap> built_serverbound;
        built_serverbound.emplace(protocol::ProtocolState::Play,
                                  PacketIdMap{"0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,"
                                              "27,28,29,30,-1,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46"});
        built_clientbound.emplace(
            protocol::ProtocolState::Play,
            PacketIdMap{"0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,"
                        "35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,15,60,61,62,63,64,"
                        "65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91"});
        built.packets = PacketIdTables{std::move(built_clientbound), std::move(built_serverbound)};
        built.blockStates =
            IdMap{"0:4729:0;4735:10155:-4;14898:2214:-8|4729>4729;4730>4730;4731>4729;4732>4730;4733>4729;4734>4730;"
                  "14890>14886;14891>14886;14892>14887;14893>14887;14894>14888;14895>14888;14896>14889;14897>14889"};
        built.item = IdMap{"0:572:0;572:11:3;583:4:4;587:1:26;588:4:3;592:1:22;593:4:9;597:1:20;598:1:-12;599:3:-27;"
                           "602:1:13;603:4:-8;607:1:9;608:4:-2;612:1:6;613:3:-14;616:3:-6;619:75:0;694:2:2;696:1:16;"
                           "697:1:10;698:1:8;699:1:6;700:1:4;701:1:2;702:1:0;703:1:-2;704:1:-4;705:1:-6;706:1:-8;707:2:"
                           "2;709:1:-14;710:1:-16;711:1:0;712:1:-4;713:78:0;792:184:-1|"};
        built.entityType = IdMap{"0:61:0;62:46:-1|61>60"};
        built.mobEffect = IdMap{"1:32:0|"};
        built.soundEvent = IdMap{"0:587:0;588:38:-1;632:360:-7|"};
        built.staticBiomes = NameIdMap{
            "minecraft:badlands:37;minecraft:badlands_plateau:39;minecraft:bamboo_jungle:168;minecraft:bamboo_jungle_"
            "hills:169;minecraft:basalt_deltas:173;minecraft:beach:16;minecraft:birch_forest:27;minecraft:birch_forest_"
            "hills:28;minecraft:cold_ocean:46;minecraft:crimson_forest:171;minecraft:dark_forest:29;minecraft:dark_"
            "forest_hills:157;minecraft:deep_cold_ocean:49;minecraft:deep_frozen_ocean:50;minecraft:deep_lukewarm_"
            "ocean:48;minecraft:deep_ocean:24;minecraft:deep_warm_ocean:47;minecraft:desert:2;minecraft:desert_hills:"
            "17;minecraft:desert_lakes:130;minecraft:end_barrens:43;minecraft:end_highlands:42;minecraft:end_midlands:"
            "41;minecraft:eroded_badlands:165;minecraft:flower_forest:132;minecraft:forest:4;minecraft:frozen_ocean:10;"
            "minecraft:frozen_river:11;minecraft:giant_spruce_taiga:160;minecraft:giant_spruce_taiga_hills:161;"
            "minecraft:giant_tree_taiga:32;minecraft:giant_tree_taiga_hills:33;minecraft:gravelly_mountains:131;"
            "minecraft:ice_spikes:140;minecraft:jungle:21;minecraft:jungle_edge:23;minecraft:jungle_hills:22;minecraft:"
            "lukewarm_ocean:45;minecraft:modified_badlands_plateau:167;minecraft:modified_gravelly_mountains:162;"
            "minecraft:modified_jungle:149;minecraft:modified_jungle_edge:151;minecraft:modified_wooded_badlands_"
            "plateau:166;minecraft:mountain_edge:20;minecraft:mountains:3;minecraft:mushroom_field_shore:15;minecraft:"
            "mushroom_fields:14;minecraft:nether_wastes:8;minecraft:ocean:0;minecraft:plains:1;minecraft:river:7;"
            "minecraft:savanna:35;minecraft:savanna_plateau:36;minecraft:shattered_savanna:163;minecraft:shattered_"
            "savanna_plateau:164;minecraft:small_end_islands:40;minecraft:snowy_beach:26;minecraft:snowy_mountains:13;"
            "minecraft:snowy_taiga:30;minecraft:snowy_taiga_hills:31;minecraft:snowy_taiga_mountains:158;minecraft:"
            "snowy_tundra:12;minecraft:soul_sand_valley:170;minecraft:stone_shore:25;minecraft:sunflower_plains:129;"
            "minecraft:swamp:6;minecraft:swamp_hills:134;minecraft:taiga:5;minecraft:taiga_hills:19;minecraft:taiga_"
            "mountains:133;minecraft:tall_birch_forest:155;minecraft:tall_birch_hills:156;minecraft:the_end:9;"
            "minecraft:the_void:127;minecraft:warm_ocean:44;minecraft:warped_forest:172;minecraft:wooded_badlands_"
            "plateau:38;minecraft:wooded_hills:18;minecraft:wooded_mountains:34"};
        built.attributeNames =
            NameIdMap{"minecraft:generic.armor:8;minecraft:generic.armor_toughness:9;minecraft:generic.attack_damage:5;"
                      "minecraft:generic.attack_knockback:6;minecraft:generic.attack_speed:7;minecraft:generic.flying_"
                      "speed:4;minecraft:generic.follow_range:1;minecraft:generic.knockback_resistance:2;minecraft:"
                      "generic.luck:10;minecraft:generic.max_health:0;minecraft:generic.movement_speed:3;minecraft:"
                      "horse.jump_strength:12;minecraft:zombie.spawn_reinforcements:11"};
        built.playerEntityType = 105;
        built.newerPlayerEntityType = 106;
        built.blockStateDirectBits = 15;
        return built;
    }();
    return tables;
}

} // namespace basalt::wire::multiversion::java
