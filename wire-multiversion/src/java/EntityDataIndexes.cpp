
#include <basalt/wire/multiversion/java/EntityDataIndexes.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

constexpr std::array<std::int32_t, 1> kTypes0 = {104};
constexpr std::array<std::int32_t, 1> kTypes1 = {55};
constexpr std::array<std::int32_t, 1> kTypes2 = {97};
constexpr std::array<std::int32_t, 1> kTypes3 = {21};
constexpr std::array<std::int32_t, 6> kTypes4 = {47, 69, 104, 112, 144, 148};
constexpr std::array<std::int32_t, 3> kTypes5 = {6, 126, 139};
constexpr std::array<std::int32_t, 1> kTypes6 = {143};
constexpr std::array<std::int32_t, 1> kTypes7 = {145};
constexpr std::array<std::int32_t, 93> kTypes8 = {
    2,   4,   5,   7,   10,  11,  14,  16,  17,  19,  20,  21,  22,  26,  27,  28,  30,  31,  32,
    36,  37,  39,  41,  42,  43,  44,  47,  55,  56,  58,  59,  60,  62,  63,  64,  65,  67,  68,
    69,  71,  79,  81,  84,  87,  88,  89,  92,  97,  98,  99,  100, 101, 102, 103, 104, 105, 110,
    111, 112, 113, 114, 115, 117, 118, 119, 120, 122, 124, 127, 130, 131, 132, 133, 134, 138, 140,
    141, 142, 143, 144, 145, 146, 148, 149, 150, 152, 153, 154, 155, 156, 157, 158, 159};
constexpr std::array<std::int32_t, 1> kTypes9 = {11};
constexpr std::array<std::int32_t, 1> kTypes10 = {42};
constexpr std::array<std::int32_t, 1> kTypes11 = {139};
constexpr std::array<std::int32_t, 1> kTypes12 = {153};
constexpr std::array<std::int32_t, 1> kTypes13 = {65};
constexpr std::array<std::int32_t, 1> kTypes14 = {102};
constexpr std::array<std::int32_t, 1> kTypes15 = {132};
constexpr std::array<std::int32_t, 1> kTypes16 = {160};
constexpr std::array<std::int32_t, 1> kTypes17 = {7};
constexpr std::array<std::int32_t, 1> kTypes18 = {62};
constexpr std::array<std::int32_t, 1> kTypes19 = {63};
constexpr std::array<std::int32_t, 1> kTypes20 = {56};
constexpr std::array<std::int32_t, 1> kTypes21 = {146};
constexpr std::array<std::int32_t, 1> kTypes22 = {2};
constexpr std::array<std::int32_t, 2> kTypes23 = {19, 20};
constexpr std::array<std::int32_t, 1> kTypes24 = {70};
constexpr std::array<std::int32_t, 1> kTypes25 = {122};
constexpr std::array<std::int32_t, 3> kTypes26 = {15, 73, 135};
constexpr std::array<std::int32_t, 1> kTypes27 = {136};
constexpr std::array<std::int32_t, 1> kTypes28 = {4};
constexpr std::array<std::int32_t, 1> kTypes29 = {152};
constexpr std::array<std::int32_t, 1> kTypes30 = {93};
constexpr std::array<std::int32_t, 1> kTypes31 = {31};
constexpr std::array<std::int32_t, 1> kTypes32 = {113};
constexpr std::array<std::int32_t, 1> kTypes33 = {26};
constexpr std::array<std::int32_t, 1> kTypes34 = {30};
constexpr std::array<std::int32_t, 1> kTypes35 = {101};
constexpr std::array<std::int32_t, 1> kTypes36 = {50};
constexpr std::array<std::int32_t, 4> kTypes37 = {61, 74, 77, 94};
constexpr std::array<std::int32_t, 1> kTypes38 = {59};
constexpr std::array<std::int32_t, 1> kTypes39 = {28};
constexpr std::array<std::int32_t, 2> kTypes40 = {89, 156};
constexpr std::array<std::int32_t, 1> kTypes41 = {156};
constexpr std::array<std::int32_t, 1> kTypes42 = {157};
constexpr std::array<std::int32_t, 1> kTypes43 = {134};
constexpr std::array<std::int32_t, 42> kTypes44 = {
    4,  7,  11, 19, 20,  21,  26,  30,  36,  37,  55,  56,  59,  62,  63,  65,  67,  79,  81,  87,  88,
    89, 92, 97, 99, 101, 105, 111, 114, 119, 120, 122, 130, 132, 133, 138, 141, 143, 145, 152, 155, 156};
constexpr std::array<std::int32_t, 1> kTypes45 = {133};
constexpr std::array<std::int32_t, 3> kTypes46 = {81, 120, 133};
constexpr std::array<std::int32_t, 158> kTypes47 = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,
    23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,
    47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,
    70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,
    93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
    118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140,
    141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160};
constexpr std::array<std::int32_t, 1> kTypes48 = {5};
constexpr std::array<std::int32_t, 1> kTypes49 = {159};
constexpr std::array<std::int32_t, 7> kTypes50 = {37, 67, 79, 88, 119, 138, 155};
constexpr std::array<std::int32_t, 1> kTypes51 = {10};
constexpr std::array<std::int32_t, 5> kTypes52 = {21, 89, 99, 152, 156};
constexpr std::array<std::int32_t, 1> kTypes53 = {92};
constexpr std::array<std::int32_t, 1> kTypes54 = {111};
constexpr std::array<std::int32_t, 1> kTypes55 = {114};
constexpr std::array<std::int32_t, 5> kTypes56 = {39, 68, 154, 157, 158};
constexpr std::array<std::int32_t, 1> kTypes57 = {14};
constexpr std::array<std::int32_t, 2> kTypes58 = {22, 127};
constexpr std::array<std::int32_t, 1> kTypes59 = {32};
constexpr std::array<std::int32_t, 1> kTypes60 = {58};
constexpr std::array<std::int32_t, 5> kTypes62 = {16, 98, 118, 131, 150};
constexpr std::array<std::int32_t, 1> kTypes63 = {148};
constexpr std::array<std::int32_t, 1> kTypes64 = {71};
constexpr std::array<std::int32_t, 1> kTypes65 = {149};
constexpr std::array<std::int32_t, 1> kTypes66 = {151};
constexpr std::array<std::int32_t, 2> kTypes67 = {41, 64};
constexpr std::array<std::int32_t, 20> kTypes68 = {0,  1,  8,  9,  12, 13, 23, 24, 34,  35,
                                                   75, 76, 82, 83, 90, 91, 95, 96, 128, 129};
constexpr std::array<std::int32_t, 7> kTypes69 = {25, 29, 57, 66, 86, 125, 137};
constexpr std::array<std::int32_t, 1> kTypes70 = {29};
constexpr std::array<std::int32_t, 1> kTypes71 = {57};
constexpr std::array<std::int32_t, 1> kTypes72 = {72};
constexpr std::array<std::int32_t, 1> kTypes73 = {6};
constexpr std::array<std::int32_t, 1> kTypes74 = {54};
constexpr std::array<std::int32_t, 2> kTypes75 = {61, 74};
constexpr std::array<std::int32_t, 1> kTypes76 = {46};
constexpr std::array<std::int32_t, 1> kTypes77 = {44};
constexpr std::array<std::int32_t, 38> kTypes78 = {4,   7,   11,  19,  20,  21,  26,  30,  36,  37,  55,  56,  59,
                                                   62,  63,  65,  67,  79,  87,  88,  89,  92,  97,  99,  101, 105,
                                                   111, 114, 119, 122, 132, 138, 141, 143, 145, 152, 155, 156};

constexpr std::array<std::int32_t, 3> kTypes79 = {36, 62, 130};
constexpr std::array<std::int32_t, 1> kTypes80 = {141};
constexpr std::array<std::int32_t, 4> kTypes81 = {27, 110, 113, 140};
constexpr std::array<std::int32_t, 1> kTypes82 = {100};
constexpr std::array<std::int32_t, 2> kTypes83 = {47, 69};
constexpr std::array<std::int32_t, 1> kTypes84 = {99};

constexpr std::int32_t kByte1_8 = 0;
constexpr std::int32_t kShort1_8 = 1;
constexpr std::int32_t kInt1_8 = 2;

constexpr std::array<std::int32_t, 161> kAllTypes = [] {
    std::array<std::int32_t, 161> all{};
    for (std::size_t type = 0; type < all.size(); ++type) {
        all[type] = static_cast<std::int32_t>(type);
    }
    return all;
}();

constexpr std::array<EntityDataIndexRule, 1> kRules110 = {{
    {5, true, kAllTypes},
}};
constexpr std::array<EntityDataIndexRule, 2> kRules315 = {{
    {7, false, kTypes74},
    {14, false, kTypes35},
}};
constexpr std::array<EntityDataIndexRule, 5> kRules316 = {{
    {12, true, kTypes83},
    {12, false, kTypes84},
    {13, false, kTypes84},
    {14, false, kTypes84},
    {15, false, kTypes84},
}};
constexpr std::array<EntityDataIndexRule, 12> kRules340 = {{
    {15, true, kTypes56},
    {13, false, kTypes80},
    {14, false, kTypes80},
    {15, false, kTypes80},
    {16, false, kTypes80},
    {17, false, kTypes80},
    {18, false, kTypes80},
    {12, false, kTypes81},
    {13, false, kTypes81},
    {12, false, kTypes82},
    {12, false, kTypes68},
    {7, false, kTypes11},
}};
constexpr std::array<EntityDataIndexRule, 18> kRules404 = {{
    {15, false, kTypes0},
    {15, false, kTypes1},
    {16, false, kTypes1},
    {17, false, kTypes1},
    {18, false, kTypes1},
    {15, false, kTypes2},
    {16, false, kTypes2},
    {17, false, kTypes2},
    {18, false, kTypes2},
    {19, false, kTypes2},
    {20, false, kTypes2},
    {18, false, kTypes3},
    {19, false, kTypes3},
    {20, false, kTypes3},
    {14, true, kTypes4},
    {9, true, kTypes5},
    {12, true, kTypes8},
    {6, true, kAllTypes},
}};
constexpr std::array<EntityDataIndexRule, 2> kRules477 = {{
    {15, true, kTypes6},
    {15, false, kTypes7},
}};
constexpr std::array<EntityDataIndexRule, 5> kRules498 = {{
    {12, true, kTypes8},
    {15, false, kTypes9},
    {16, false, kTypes9},
    {16, false, kTypes10},
    {10, false, kTypes11},
}};
constexpr std::array<EntityDataIndexRule, 9> kRules578 = {{
    {16, false, kTypes12},
    {15, false, kTypes13},
    {16, false, kTypes14},
    {17, false, kTypes14},
    {18, false, kTypes14},
    {16, false, kTypes15},
    {17, false, kTypes15},
    {18, false, kTypes15},
    {8, false, kTypes16},
}};
constexpr std::array<EntityDataIndexRule, 6> kRules754 = {{
    {17, false, kTypes17},
    {18, false, kTypes17},
    {19, false, kTypes17},
    {16, false, kTypes18},
    {17, false, kTypes19},
    {7, true, kAllTypes},
}};
constexpr std::array<EntityDataIndexRule, 6> kRules758 = {{
    {16, false, kTypes20},
    {17, false, kTypes20},
    {18, false, kTypes20},
    {16, false, kTypes21},
    {18, false, kTypes19},
    {19, false, kTypes19},
}};
constexpr std::array<EntityDataIndexRule, 2> kRules759 = {{
    {16, false, kTypes22},
    {17, false, kTypes22},
}};
constexpr std::array<EntityDataIndexRule, 2> kRules760 = {{
    {19, false, kTypes23},
    {20, false, kTypes23},
}};
constexpr std::array<EntityDataIndexRule, 5> kRules761 = {{
    {8, false, kTypes24},
    {9, false, kTypes24},
    {10, false, kTypes24},
    {17, false, kTypes25},
    {18, false, kTypes25},
}};
constexpr std::array<EntityDataIndexRule, 1> kRules763 = {{
    {10, true, kTypes26},
}};
constexpr std::array<EntityDataIndexRule, 1> kRules764 = {{
    {9, true, kTypes27},
}};
constexpr std::array<EntityDataIndexRule, 3> kRules765 = {{
    {17, true, kTypes28},
    {22, true, kTypes29},
    {8, true, kTypes30},
}};
constexpr std::array<EntityDataIndexRule, 5> kRules767 = {{
    {16, true, kTypes79},
    {17, false, kTypes31},
    {16, false, kTypes31},
    {17, true, kTypes32},
    {10, true, kTypes5},
}};
constexpr std::array<EntityDataIndexRule, 2> kRules768 = {{
    {19, true, kTypes31},
    {18, true, kTypes31},
}};
constexpr std::array<EntityDataIndexRule, 5> kRules769 = {{
    {17, false, kTypes33},
    {17, false, kTypes34},
    {18, false, kTypes35},
    {23, false, kTypes29},
    {8, false, kTypes36},
}};
constexpr std::array<EntityDataIndexRule, 4> kRules770 = {{
    {8, true, kTypes37},
    {17, false, kTypes38},
    {18, false, kTypes38},
    {16, false, kTypes38},
}};
constexpr std::array<EntityDataIndexRule, 2> kRules772 = {{
    {16, false, kTypes39},
    {17, false, kTypes39},
}};
constexpr std::array<EntityDataIndexRule, 4> kRules773 = {{
    {17, false, kTypes40},
    {18, false, kTypes40},
    {19, false, kTypes40},
    {20, false, kTypes41},
}};
constexpr std::array<EntityDataIndexRule, 8> kRules774 = {{
    {21, true, kTypes42},
    {20, true, kTypes6},
    {24, true, kTypes3},
    {19, true, kTypes33},
    {20, true, kTypes35},
    {19, true, kTypes34},
    {17, true, kTypes43},
    {17, true, kTypes44},
}};
constexpr std::array<EntityDataIndexRule, 4> kRules775 = {{
    {19, false, kTypes45},
    {20, false, kTypes45},
    {17, true, kTypes46},
    {16, true, kTypes46},
}};

constexpr std::array<EntityDataIndexMove, 81> kMoves47 = {{
    {0, 0, kTypes47},
    {1, 1, kTypes47, kShort1_8},
    {2, 2, kTypes47},
    {3, 3, kTypes47},
    {4, 4, kTypes47},
    {6, 6, kTypes8},
    {7, 7, kTypes8, kInt1_8},
    {8, 8, kTypes8},
    {9, 9, kTypes8, kByte1_8},
    {10, 10, kTypes48},
    {10, 15, kTypes8},
    {11, 12, kTypes78, kEntityDataChildByte},
    {11, 11, kTypes48},
    {12, 12, kTypes48},
    {13, 13, kTypes48},
    {14, 14, kTypes48},
    {15, 15, kTypes48},
    {16, 16, kTypes48},
    {12, 10, kTypes49},
    {16, 16, kTypes49},
    {10, 17, kTypes49},
    {11, 18, kTypes49, kInt1_8},
    {5, 5, kTypes49},
    {12, 16, kTypes50, kInt1_8},
    {13, 19, kTypes50, kByte1_8},
    {14, 20, kTypes50, kInt1_8},
    {15, 21, kTypes50},
    {16, 22, kTypes50, kInt1_8},
    {11, 16, kTypes51},
    {12, 16, kTypes52},
    {13, 17, kTypes52},
    {14, 18, kTypes53, kByte1_8},
    {14, 18, kTypes29},
    {15, 19, kTypes29},
    {16, 20, kTypes29, kByte1_8},
    {12, 16, kTypes35},
    {12, 18, kTypes54, kByte1_8},
    {12, 16, kTypes55},
    {12, 16, kTypes6, kInt1_8},
    {11, 16, kTypes10, kShort1_8},
    {17, 17, kTypes10},
    {12, 18, kTypes10},
    {11, 12, kTypes56},
    {12, 13, kTypes56, kByte1_8},
    {13, 14, kTypes56},
    {11, 16, kTypes57},
    {11, 16, kTypes58},
    {11, 16, kTypes59, kByte1_8},
    {12, 17, kTypes59},
    {13, 18, kTypes59},
    {11, 16, kTypes60},
    {11, 16, kTypes46, kByte1_8},
    {11, 13, kTypes62, kByte1_8},
    {11, 21, kTypes63},
    {11, 16, kTypes64},
    {11, 17, kTypes65, kInt1_8},
    {12, 18, kTypes65, kInt1_8},
    {13, 19, kTypes65, kInt1_8},
    {14, 20, kTypes65, kInt1_8},
    {5, 10, kTypes66},
    {11, 16, kTypes67, kInt1_8},
    {12, 17, kTypes67, kInt1_8},
    {5, 17, kTypes68, kInt1_8},
    {6, 18, kTypes68, kInt1_8},
    {7, 19, kTypes68},
    {5, 17, kTypes69, kInt1_8},
    {6, 18, kTypes69, kInt1_8},
    {7, 19, kTypes69},
    {8, 20, kTypes69, kInt1_8},
    {9, 21, kTypes69, kInt1_8},
    {10, 22, kTypes69},
    {11, 23, kTypes70},
    {12, 24, kTypes70},
    {11, 16, kTypes71},
    {5, 10, kTypes72},
    {5, 16, kTypes73},
    {5, 8, kTypes74},
    {5, 8, kTypes75},
    {6, 9, kTypes75, kByte1_8},
    {8, 8, kTypes76},
    {11, 11, kTypes77},
}};

constexpr std::array<EntityDataIndexMove, 4> kSwaps772 = {{
    {15, 18, kTypes49},
    {16, 17, kTypes49},
    {17, 15, kTypes49},
    {18, 16, kTypes49},
}};

} // namespace

std::span<const EntityDataIndexRule> entityDataIndexRules(std::int32_t version) {
    switch (version) {
    case 110:
        return kRules110;
    case 315:
        return kRules315;
    case 316:
        return kRules316;
    case 340:
        return kRules340;
    case 404:
        return kRules404;
    case 477:
        return kRules477;
    case 498:
        return kRules498;
    case 578:
        return kRules578;
    case 754:
        return kRules754;
    case 758:
        return kRules758;
    case 759:
        return kRules759;
    case 760:
        return kRules760;
    case 761:
        return kRules761;
    case 763:
        return kRules763;
    case 764:
        return kRules764;
    case 765:
        return kRules765;
    case 767:
        return kRules767;
    case 768:
        return kRules768;
    case 769:
        return kRules769;
    case 770:
        return kRules770;
    case 772:
        return kRules772;
    case 773:
        return kRules773;
    case 774:
        return kRules774;
    case 775:
        return kRules775;
    default:
        return {};
    }
}

std::span<const EntityDataIndexMove> entityDataIndexMoves(std::int32_t version) {
    switch (version) {
    case 47:
        return kMoves47;
    default:
        return {};
    }
}

std::span<const EntityDataIndexMove> entityDataIndexSwaps(std::int32_t version) {
    return version == 772 ? std::span<const EntityDataIndexMove>{kSwaps772} : std::span<const EntityDataIndexMove>{};
}

} // namespace basalt::wire::multiversion::java
