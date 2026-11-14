#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/java/Content340.hpp>
#include <basalt/wire/multiversion/java/Flattening340.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::wire::multiversion::java::content340 {
namespace {

namespace flat = flattening340;

constexpr std::int16_t kEmpty = -1;
constexpr std::int32_t kSpawnEgg = 383;
constexpr std::int32_t kFilledMap = 358;
constexpr std::int32_t kBanner = 425;
constexpr std::int32_t kShield = 442;
constexpr std::int32_t kWhite = 15;
constexpr std::int16_t kStone = 1;
constexpr std::int32_t kReddust = 30;
constexpr std::int32_t kIconCrack = 36;
constexpr std::int32_t kBlockCrack = 37;
constexpr std::int32_t kBlockDust = 38;
constexpr std::int32_t kFallingDust = 46;

[[nodiscard]] bool damageable(std::int32_t id) { return std::ranges::binary_search(flat::damageable(), id); }

[[nodiscard]] std::int32_t eggFor(std::string_view entity) {
    const auto eggs = flat::spawnEggs();
    const auto found = std::ranges::lower_bound(eggs, entity, {}, &flat::NamedId::name);
    return found != eggs.end() && found->name == entity ? found->id : -1;
}

[[nodiscard]] std::string_view entityOfEgg(std::int32_t item) {
    for (const flat::NamedId& egg : flat::spawnEggs()) {
        if (egg.id == item) {
            return egg.name;
        }
    }
    return {};
}

[[nodiscard]] std::string enchantmentName(std::int32_t id) {
    for (const flat::NamedId& one : flat::enchantments()) {
        if (one.id == id) {
            return std::string{one.name};
        }
    }
    return {};
}

[[nodiscard]] std::int32_t enchantmentId(std::string_view name) {
    constexpr std::string_view kNamespace = "minecraft:";
    for (const flat::NamedId& one : flat::enchantments()) {
        if (one.name == name || one.name.substr(kNamespace.size()) == name) {
            return one.id;
        }
    }
    return -1;
}

[[nodiscard]] nbt::NbtTag renamedEnchantments(const nbt::NbtList& list, bool up) {
    nbt::NbtList out{nbt::NbtType::Compound};
    for (const nbt::NbtTag& entry : list) {
        const nbt::NbtCompound* one = entry.get<nbt::NbtCompound>();
        if (one == nullptr) {
            continue;
        }
        nbt::NbtCompound renamed;
        if (up) {
            const std::string name = enchantmentName(one->getShort("id"));
            if (name.empty()) {
                continue;
            }
            renamed.putString("id", name);
        } else {
            const std::int32_t id = enchantmentId(one->getString("id"));
            if (id < 0) {
                continue;
            }
            renamed.putShort("id", static_cast<std::int16_t>(id));
        }
        renamed.putShort("lvl", one->getShort("lvl"));
        static_cast<void>(out.push(nbt::NbtTag::ofCompound(std::move(renamed))));
    }
    return nbt::NbtTag::ofList(std::move(out));
}

void moveEnchantments(nbt::NbtCompound& tag, std::string_view from, std::string_view to, bool up) {
    if (const nbt::NbtList* list = tag.getList(from)) {
        nbt::NbtTag renamed = renamedEnchantments(*list, up);
        tag.remove(from);
        tag.set(to, std::move(renamed));
    }
}

/// parity: ItemBannerColorFix, patterns count dye damage in 1.12 and colours in 1.13.
void flipPatterns(nbt::NbtCompound& tag, bool removeBase) {
    nbt::NbtTag* entity = tag.find("BlockEntityTag");
    nbt::NbtCompound* block = entity != nullptr ? entity->get<nbt::NbtCompound>() : nullptr;
    if (block == nullptr) {
        return;
    }
    if (nbt::NbtTag* patterns = block->find("Patterns")) {
        if (nbt::NbtList* list = patterns->get<nbt::NbtList>()) {
            for (std::size_t index = 0; index < list->size(); ++index) {
                if (nbt::NbtCompound* one = (*list)[index].get<nbt::NbtCompound>()) {
                    one->putInt("Color", kWhite - one->getInt("Color"));
                }
            }
        }
    }
    if (removeBase) {
        block->remove("Base");
    } else if (block->contains("Base")) {
        block->putInt("Base", kWhite - block->getInt("Base"));
    }
}

void renameDisplay(nbt::NbtCompound& tag, bool up) {
    nbt::NbtTag* display = tag.find("display");
    nbt::NbtCompound* compound = display != nullptr ? display->get<nbt::NbtCompound>() : nullptr;
    if (compound == nullptr || !compound->contains("Name")) {
        return;
    }
    const std::string name{compound->getString("Name")};
    if (up) {
        compound->putString("Name", legacyToJson(name));
        return;
    }
    const auto parsed = common::parseJson(name);
    if (parsed) {
        compound->putString("Name", text::toLegacyText(text::componentJson::deserialize(*parsed)));
    }
}

void writeTag(io::ByteBuffer& out, nbt::NbtCompound tag) {
    if (tag.empty()) {
        out.writeByte(0);
        return;
    }
    writeNamedNbt(out, nbt::NbtTag::ofCompound(std::move(tag)));
}

[[nodiscard]] common::JsonValue renamed(const common::JsonValue& value, bool& changed) {
    if (const auto* list = value.get<std::vector<common::JsonValue>>()) {
        std::vector<common::JsonValue> out;
        out.reserve(list->size());
        for (const common::JsonValue& one : *list) {
            out.push_back(renamed(one, changed));
        }
        return common::JsonValue::ofArray(std::move(out));
    }
    const auto* object = value.get<common::JsonObject>();
    if (object == nullptr) {
        return value;
    }
    common::JsonObject out;
    for (std::size_t index = 0; index < object->size(); ++index) {
        const std::string& name = object->names()[index];
        const common::JsonValue& member = object->values()[index];
        const auto* key = member.get<std::string>();
        if (name == "translate" && key != nullptr) {
            const auto keys = flat::translations();
            const auto found = std::ranges::lower_bound(keys, std::string_view{*key}, {}, &flat::Rename::from);
            if (found != keys.end() && found->from == *key) {
                out.set(name, common::JsonValue::of(std::string{found->to}));
                changed = true;
                continue;
            }
        }
        out.set(name, renamed(member, changed));
    }
    return common::JsonValue::ofObject(std::move(out));
}

} // namespace

std::string renamedKeys(std::string_view json) {
    if (json.find("\"translate\"") == std::string_view::npos) {
        return {};
    }
    const auto parsed = common::parseJson(json);
    if (!parsed) {
        return {};
    }
    bool changed = false;
    common::JsonValue out = renamed(*parsed, changed);
    return changed ? common::writeJson(out) : std::string{};
}

std::int32_t blockState(std::int32_t legacy) {
    static const std::array<std::int32_t, 4096> states = [] {
        std::array<std::int32_t, 4096> built{};
        for (std::int32_t one = 0; one < static_cast<std::int32_t>(built.size()); ++one) {
            std::int32_t state = flat::blockStates().map(one);
            if (state == IdMap::kNone) {
                state = flat::blockStates().map(one & ~15);
            }
            built[static_cast<std::size_t>(one)] = state == IdMap::kNone ? 0 : state;
        }
        return built;
    }();
    return states[static_cast<std::size_t>(legacy & 4095)];
}

std::int32_t blockStateOfStateId(std::int32_t value) {
    return blockState(((value & 4095) << 4) | ((value >> 12) & 15));
}

std::int32_t particleId(std::int32_t legacy) {
    const auto table = flat::particles();
    return legacy >= 0 && static_cast<std::size_t>(legacy) < table.size() ? table[static_cast<std::size_t>(legacy)]
                                                                          : -1;
}

void particleData(io::ByteBuffer& out, std::int32_t legacy, std::span<const std::int32_t> data,
                  std::span<const float> colour) {
    const auto at = [&data](std::size_t index) { return index < data.size() ? data[index] : 0; };
    switch (legacy) {
    case kReddust:
        for (const float part : colour) {
            out.writeFloat(part);
        }
        break;
    case kIconCrack: {
        const std::int32_t item = upItem(at(0), at(1));
        out.writeShort(static_cast<std::int16_t>(item == IdMap::kNone ? kStone : item));
        out.writeByte(1);
        out.writeByte(0);
        break;
    }
    case kBlockCrack:
    case kBlockDust:
    case kFallingDust:
        protocol::varInt::write(out, blockStateOfStateId(at(0)));
        break;
    default:
        break;
    }
}

bool copyEntityValue(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t type) {
    constexpr std::int32_t kSlot = 5;
    constexpr std::int32_t kNbt = 13;
    switch (type) {
    case 0:
    case 6:
        out.writeByte(in.readByte());
        return true;
    case 1:
    case 10:
    case 12:
        protocol::varInt::write(out, protocol::varInt::read(in));
        return true;
    case 2:
        out.writeFloat(in.readFloat());
        return true;
    case 3:
    case 4:
        protocol::writeString(out, protocol::readString(in));
        return true;
    case kSlot: {
        const std::int16_t id = in.readShort();
        out.writeShort(id);
        if (id != kEmpty) {
            out.writeBytes(in.readSpan(3));
            const auto tag = readNamedNbt(in);
            if (tag) {
                out.writeBytes(tag->raw);
            }
        }
        return true;
    }
    case 7:
        out.writeBytes(in.readSpan(12));
        return true;
    case 8:
        out.writeLong(in.readLong());
        return true;
    case 9:
    case 11: {
        const bool present = protocol::readBoolean(in);
        protocol::writeBoolean(out, present);
        if (present) {
            out.writeBytes(in.readSpan(type == 9 ? 8 : 16));
        }
        return true;
    }
    case kNbt: {
        const auto tag = readNamedNbt(in);
        if (tag) {
            out.writeBytes(tag->raw);
        }
        return true;
    }
    default:
        return false;
    }
}

std::string legacyToJson(std::string_view legacy) {
    return common::writeJson(text::componentJson::serialize(text::Component::text(std::string{legacy})));
}

void upLegacyStack(io::ByteBuffer& in, io::ByteBuffer& out) {
    constexpr std::int32_t kPotion = 373;
    constexpr std::int32_t kSplashPotion = 438;
    constexpr std::int32_t kSplash = 16384;
    const std::int16_t id = in.readShort();
    if (id == kEmpty) {
        out.writeShort(id);
        return;
    }
    const std::uint8_t count = in.readByte();
    const std::int16_t damage = in.readShort();
    const auto named = readNamedNbt(in);
    if (!named) {
        return;
    }
    if (id != kPotion && id != kSpawnEgg) {
        out.writeShort(id);
        out.writeByte(count);
        out.writeShort(damage);
        out.writeBytes(named->raw);
        return;
    }
    const nbt::NbtCompound* read = named->tag.get<nbt::NbtCompound>();
    nbt::NbtCompound tag = read != nullptr ? *read : nbt::NbtCompound{};
    std::int16_t item = id;
    if (id == kPotion) {
        const std::string_view potion = flat::potions18()[static_cast<std::size_t>(damage & 127)];
        tag.putString("Potion", std::string{potion.empty() ? "minecraft:water" : potion});
        item = (damage & kSplash) != 0 ? kSplashPotion : kPotion;
    } else {
        const std::string_view entity = flat::eggs18()[static_cast<std::size_t>(damage & 255)];
        if (!entity.empty()) {
            nbt::NbtCompound entityTag;
            entityTag.putString("id", std::string{entity});
            tag.set("EntityTag", nbt::NbtTag::ofCompound(std::move(entityTag)));
        }
    }
    out.writeShort(item);
    out.writeByte(count);
    out.writeShort(0);
    writeTag(out, std::move(tag));
}

void downLegacyStack(io::ByteBuffer& in, io::ByteBuffer& out) {
    constexpr std::int32_t kPotion = 373;
    constexpr std::int32_t kSplashPotion = 438;
    constexpr std::int32_t kSplash = 16384;
    const std::int16_t id = in.readShort();
    const bool potion = id == kPotion || id == kSplashPotion;
    out.writeShort(potion ? static_cast<std::int16_t>(kPotion) : id);
    if (id == kEmpty) {
        return;
    }
    const std::uint8_t count = in.readByte();
    std::int16_t damage = in.readShort();
    const auto named = readNamedNbt(in);
    if (!named) {
        return;
    }
    const nbt::NbtCompound* tag = named->tag.get<nbt::NbtCompound>();
    if (potion && tag != nullptr) {
        const auto names = flat::potions18();
        const auto found = std::ranges::find(names, tag->getString("Potion"));
        damage = static_cast<std::int16_t>((found == names.end() ? 0 : found - names.begin()) |
                                           (id == kSplashPotion ? kSplash : 0));
    } else if (id == kSpawnEgg && tag != nullptr && tag->getCompound("EntityTag") != nullptr) {
        const auto names = flat::eggs18();
        const auto found = std::ranges::find(names, tag->getCompound("EntityTag")->getString("id"));
        damage = static_cast<std::int16_t>(found == names.end() ? 0 : found - names.begin());
    }
    out.writeByte(count);
    out.writeShort(damage);
    out.writeBytes(named->raw);
}

std::int32_t upItem(std::int32_t id, std::int32_t damage) {
    if (damage >= 0 && damage < 16 && !damageable(id)) {
        const std::int32_t exact = flat::items().map(id << 4 | damage);
        if (exact != IdMap::kNone) {
            return exact;
        }
    }
    return flat::items().map(id << 4);
}

void upStack(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::int16_t id = in.readShort();
    if (id == kEmpty) {
        out.writeShort(kEmpty);
        return;
    }
    const std::uint8_t count = in.readByte();
    const std::int16_t damage = in.readShort();
    const auto named = readNamedNbt(in);
    if (in.failed()) {
        return;
    }
    const nbt::NbtCompound* read = named->tag.get<nbt::NbtCompound>();
    nbt::NbtCompound tag = read != nullptr ? *read : nbt::NbtCompound{};
    std::int32_t item = upItem(id, damage);
    if (damageable(id) && damage != 0) {
        tag.putInt("Damage", damage);
    }
    if (id == kSpawnEgg) {
        const nbt::NbtCompound* entity = tag.getCompound("EntityTag");
        const std::int32_t egg = entity != nullptr ? eggFor(entity->getString("id")) : -1;
        if (egg >= 0) {
            item = egg;
        }
    } else if (id == kFilledMap) {
        tag.putInt("map", damage);
    } else if (id == kBanner || id == kShield) {
        flipPatterns(tag, id == kBanner);
    }
    moveEnchantments(tag, "ench", "Enchantments", true);
    moveEnchantments(tag, "StoredEnchantments", "StoredEnchantments", true);
    renameDisplay(tag, true);
    if (item == IdMap::kNone) {
        out.writeShort(kEmpty);
        return;
    }
    out.writeShort(static_cast<std::int16_t>(item));
    out.writeByte(count);
    writeTag(out, std::move(tag));
}

void downStack(io::ByteBuffer& in, io::ByteBuffer& out) {
    const std::int16_t item = in.readShort();
    if (item == kEmpty) {
        out.writeShort(kEmpty);
        return;
    }
    const std::uint8_t count = in.readByte();
    const auto named = readNamedNbt(in);
    if (in.failed()) {
        return;
    }
    const nbt::NbtCompound* read = named->tag.get<nbt::NbtCompound>();
    nbt::NbtCompound tag = read != nullptr ? *read : nbt::NbtCompound{};
    const std::int32_t legacy = flat::itemsDown().map(item);
    if (legacy == IdMap::kNone) {
        out.writeShort(kEmpty);
        return;
    }
    const std::int32_t id = legacy >> 4;
    std::int32_t damage = legacy & 15;
    if (damageable(id)) {
        damage = tag.getInt("Damage");
        tag.remove("Damage");
    }
    if (id == kSpawnEgg) {
        const std::string_view entity = entityOfEgg(item);
        nbt::NbtCompound entityTag =
            tag.getCompound("EntityTag") != nullptr ? *tag.getCompound("EntityTag") : nbt::NbtCompound{};
        if (!entity.empty() && !entityTag.contains("id")) {
            entityTag.putString("id", std::string{entity});
        }
        tag.set("EntityTag", nbt::NbtTag::ofCompound(std::move(entityTag)));
    } else if (id == kFilledMap && tag.contains("map")) {
        damage = tag.getInt("map");
        tag.remove("map");
    } else if (id == kBanner || id == kShield) {
        flipPatterns(tag, false);
    }
    moveEnchantments(tag, "Enchantments", "ench", false);
    moveEnchantments(tag, "StoredEnchantments", "StoredEnchantments", false);
    renameDisplay(tag, false);
    out.writeShort(static_cast<std::int16_t>(id));
    out.writeByte(count);
    out.writeShort(static_cast<std::int16_t>(damage));
    writeTag(out, std::move(tag));
}

} // namespace basalt::wire::multiversion::java::content340
