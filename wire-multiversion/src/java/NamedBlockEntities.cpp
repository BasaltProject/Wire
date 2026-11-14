#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/java/BlockEntityTypes.hpp>
#include <basalt/wire/multiversion/java/NamedBlockEntities.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <array>
#include <string>
#include <utility>

namespace basalt::wire::multiversion::java::namedBlockEntities {
namespace {

constexpr std::string_view kSign = "minecraft:sign";
constexpr std::size_t kSignLines = 4;
constexpr std::string_view kEmptyLine = R"({"text":""})";

// parity: ClientboundBlockEntityDataPacket, the type each block entity sent its update with up to 1.17.1.
constexpr std::array<std::pair<std::string_view, std::uint8_t>, 12> kActions = {{
    {"minecraft:mob_spawner", 1},
    {"minecraft:command_block", 2},
    {"minecraft:beacon", 3},
    {"minecraft:skull", 4},
    {"minecraft:conduit", 5},
    {"minecraft:banner", 6},
    {"minecraft:structure_block", 7},
    {"minecraft:end_gateway", 8},
    {kSign, 9},
    {"minecraft:bed", 11},
    {"minecraft:jigsaw", 12},
    {"minecraft:campfire", 13},
}};

// parity: TileEntity names of 1.10, which 1.11 replaced with registry names.
constexpr std::array<std::pair<std::string_view, std::string_view>, 24> kNamesBefore1_11 = {{
    {"minecraft:furnace", "Furnace"},
    {"minecraft:chest", "Chest"},
    {"minecraft:trapped_chest", "Chest"},
    {"minecraft:ender_chest", "EnderChest"},
    {"minecraft:jukebox", "RecordPlayer"},
    {"minecraft:dispenser", "Trap"},
    {"minecraft:dropper", "Dropper"},
    {kSign, "Sign"},
    {"minecraft:mob_spawner", "MobSpawner"},
    {"minecraft:noteblock", "Music"},
    {"minecraft:piston", "Piston"},
    {"minecraft:brewing_stand", "Cauldron"},
    {"minecraft:enchanting_table", "EnchantTable"},
    {"minecraft:end_portal", "Airportal"},
    {"minecraft:beacon", "Beacon"},
    {"minecraft:skull", "Skull"},
    {"minecraft:daylight_detector", "DLDetector"},
    {"minecraft:hopper", "Hopper"},
    {"minecraft:comparator", "Comparator"},
    {"minecraft:flower_pot", "FlowerPot"},
    {"minecraft:banner", "Banner"},
    {"minecraft:structure_block", "Structure"},
    {"minecraft:end_gateway", "EndGateway"},
    {"minecraft:command_block", "Control"},
}};

std::string lineAsJson(const nbt::NbtTag& line) {
    return common::writeJson(text::componentJson::serializeLegacy(text::componentNbt::deserialize(line)));
}

} // namespace

nbt::NbtCompound tagFor(std::int32_t nativeType, const nbt::NbtCompound* tag, std::int32_t x, std::int32_t y,
                        std::int32_t z) {
    nbt::NbtCompound out = tag == nullptr ? nbt::NbtCompound{} : *tag;
    const std::string_view name = blockEntityTypes::nativeName(nativeType);
    out.putString("id", std::string{name});
    out.putInt("x", x);
    out.putInt("y", y);
    out.putInt("z", z);
    return out;
}

void useNameBefore1_11(nbt::NbtCompound& tag) {
    const std::string_view modern = tag.getString("id");
    for (const auto& [name, legacy] : kNamesBefore1_11) {
        if (name == modern) {
            tag.putString("id", std::string{legacy});
            return;
        }
    }
}

bool signLinesAsJson(nbt::NbtCompound& tag) {
    bool sign = false;
    for (const std::string_view side : {"front_text", "back_text"}) {
        nbt::NbtTag* text = tag.find(side);
        auto* compound = text == nullptr ? nullptr : text->get<nbt::NbtCompound>();
        if (compound == nullptr) {
            continue;
        }
        sign = true;
        for (const std::string_view key : {"messages", "filtered_messages"}) {
            const nbt::NbtList* lines = compound->getList(key);
            if (lines == nullptr) {
                continue;
            }
            nbt::NbtList json{nbt::NbtType::String, {}};
            for (std::size_t index = 0; index < lines->size(); ++index) {
                json.push(nbt::NbtTag::ofString(lineAsJson((*lines)[index])));
            }
            compound->set(key, nbt::NbtTag::ofList(std::move(json)));
        }
    }
    return sign;
}

bool signAsOneSide(nbt::NbtCompound& tag) {
    const nbt::NbtCompound* front = tag.getCompound("front_text");
    if (front == nullptr) {
        return false;
    }
    nbt::NbtCompound lines;
    const nbt::NbtList* messages = front->getList("messages");
    for (std::size_t index = 0; index < kSignLines; ++index) {
        const nbt::NbtTag* line = messages != nullptr && index < messages->size() ? &(*messages)[index] : nullptr;
        const std::string* json = line == nullptr ? nullptr : line->get<std::string>();
        lines.putString("Text" + std::to_string(index + 1), json != nullptr ? *json : std::string{kEmptyLine});
    }
    lines.putString("Color", std::string{front->getString("color", "black")});
    lines.putBoolean("GlowingText", front->getBoolean("has_glowing_text"));
    tag.remove("front_text");
    tag.remove("back_text");
    tag.remove("is_waxed");
    for (std::size_t index = 0; index < lines.size(); ++index) {
        tag.set(lines.names()[index], lines.values()[index]);
    }
    return true;
}

std::uint8_t actionFor(std::string_view name) {
    for (const auto& [type, action] : kActions) {
        if (type == name) {
            return action;
        }
    }
    return kNoAction;
}

} // namespace basalt::wire::multiversion::java::namedBlockEntities
