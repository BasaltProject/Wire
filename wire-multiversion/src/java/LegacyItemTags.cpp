#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/LegacyItemTags.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace basalt::wire::multiversion::java::legacyItemTags {
namespace {

using protocol::play::ItemComponent;
using protocol::play::WireItemStack;
namespace component = protocol::play::dataComponent;
namespace versions = protocolVersions;

constexpr std::int32_t kAttributeModifiers = 16;
constexpr std::int32_t kCanPlaceOn = 14;
constexpr std::int32_t kCanBreak = 15;
constexpr std::int32_t kTooltipDisplay = 18;
constexpr std::int32_t kPotionContents = 53;
constexpr std::int32_t kWritableBook = 56;
constexpr std::int32_t kWrittenBook = 57;

constexpr std::string_view kNamespace = "minecraft:";

// parity: Potions, in the order the 26.3 built-in registry numbers them.
constexpr std::array<std::string_view, 46> kPotions = {
    "water",
    "mundane",
    "thick",
    "awkward",
    "night_vision",
    "long_night_vision",
    "invisibility",
    "long_invisibility",
    "leaping",
    "long_leaping",
    "strong_leaping",
    "fire_resistance",
    "long_fire_resistance",
    "swiftness",
    "long_swiftness",
    "strong_swiftness",
    "slowness",
    "long_slowness",
    "strong_slowness",
    "turtle_master",
    "long_turtle_master",
    "strong_turtle_master",
    "water_breathing",
    "long_water_breathing",
    "healing",
    "strong_healing",
    "harming",
    "strong_harming",
    "poison",
    "long_poison",
    "strong_poison",
    "regeneration",
    "long_regeneration",
    "strong_regeneration",
    "strength",
    "long_strength",
    "strong_strength",
    "weakness",
    "long_weakness",
    "luck",
    "slow_falling",
    "long_slow_falling",
    "wind_charged",
    "weaving",
    "oozing",
    "infested",
};

// The 1.8 data value of each potion above; 0 is a water bottle, as 1.8 had nothing closer.
constexpr std::array<std::int16_t, 46> kPotionDamage = {
    0,    64,   32,   16,   8198, 8262, 8206, 8270, 8203, 8267, 8235, 8195, 8259, 8194, 8258, 8226,
    8202, 8266, 8202, 0,    0,    0,    8205, 8269, 8261, 8229, 8204, 8236, 8196, 8260, 8228, 8193,
    8257, 8225, 8201, 8265, 8233, 8200, 8264, 8203, 0,    0,    0,    0,    0,    0,
};

constexpr std::int32_t kSplashBit = 8192;
constexpr std::int32_t kSplashPotion = 1442;
constexpr std::int32_t kLingeringPotion = 1445;

struct NumberedEnchantment {
    std::string_view name;
    std::int16_t id;
};

// parity: Enchantment.REGISTRY before 1.13 numbered enchantments by hand.
constexpr std::array<NumberedEnchantment, 30> kNumberedEnchantments = {{
    {"protection", 0},
    {"fire_protection", 1},
    {"feather_falling", 2},
    {"blast_protection", 3},
    {"projectile_protection", 4},
    {"respiration", 5},
    {"aqua_affinity", 6},
    {"thorns", 7},
    {"depth_strider", 8},
    {"frost_walker", 9},
    {"binding_curse", 10},
    {"sharpness", 16},
    {"smite", 17},
    {"bane_of_arthropods", 18},
    {"knockback", 19},
    {"fire_aspect", 20},
    {"looting", 21},
    {"sweeping", 22},
    {"efficiency", 32},
    {"silk_touch", 33},
    {"unbreaking", 34},
    {"fortune", 35},
    {"power", 48},
    {"punch", 49},
    {"flame", 50},
    {"infinity", 51},
    {"luck_of_the_sea", 61},
    {"lure", 62},
    {"mending", 70},
    {"vanishing_curse", 71},
}};

constexpr std::string_view kSweepingEdge = "sweeping_edge";
constexpr std::string_view kSweeping = "sweeping";

std::string_view bare(std::string_view key) {
    return key.starts_with(kNamespace) ? key.substr(kNamespace.size()) : key;
}

/// The name an enchantment had below 1.20.5, without its namespace.
std::string_view olderName(std::string_view key) {
    const std::string_view name = bare(key);
    return name == kSweepingEdge ? kSweeping : name;
}

std::optional<std::int32_t> numberOf(const nbt::NbtTag* tag) {
    if (tag == nullptr) {
        return std::nullopt;
    }
    if (const auto* value = tag->get<std::int8_t>()) {
        return *value;
    }
    if (const auto* value = tag->get<std::int16_t>()) {
        return *value;
    }
    if (const auto* value = tag->get<std::int32_t>()) {
        return *value;
    }
    return std::nullopt;
}

std::string nameText(const text::Component& value, bool json) {
    return json ? common::writeJson(text::componentJson::serializeLegacy(value)) : text::toLegacyText(value);
}

/// A string with formatting codes, which only clients below 1.13 send.
text::Component fromCodes(std::string_view value) {
    static constexpr std::string_view kSection = "\xC2\xA7";
    static constexpr std::array<std::string_view, 16> kColors = {
        "black",     "dark_blue", "dark_green", "dark_aqua", "dark_red", "dark_purple",  "gold",   "gray",
        "dark_gray", "blue",      "green",      "aqua",      "red",      "light_purple", "yellow", "white"};
    static constexpr std::string_view kCodes = "0123456789abcdef";
    std::vector<text::Component> parts;
    text::Style style;
    std::size_t start = 0;
    auto flush = [&](std::size_t end) {
        if (end > start) {
            parts.push_back(text::Component::text(std::string{value.substr(start, end - start)}).withStyle(style));
        }
    };
    for (std::size_t at = value.find(kSection); at != std::string_view::npos; at = value.find(kSection, start)) {
        flush(at);
        start = std::min(value.size(), at + kSection.size() + 1);
        if (at + kSection.size() >= value.size()) {
            break;
        }
        const char code = static_cast<char>(std::tolower(static_cast<unsigned char>(value[at + kSection.size()])));
        if (const std::size_t color = kCodes.find(code); color != std::string_view::npos) {
            style = text::Style{};
            style.color = text::TextColor::named(kColors[color]);
        } else if (code == 'l') {
            style.bold = true;
        } else if (code == 'm') {
            style.strikethrough = true;
        } else if (code == 'n') {
            style.underlined = true;
        } else if (code == 'o') {
            style.italic = true;
        } else if (code == 'k') {
            style.obfuscated = true;
        } else if (code == 'r') {
            style = text::Style{};
        }
    }
    flush(value.size());
    if (parts.size() == 1) {
        return parts.front();
    }
    return text::Component::text("").withChildren(std::move(parts));
}

text::Component fromText(std::string_view value, bool json) {
    if (!json) {
        return fromCodes(value);
    }
    const auto parsed = common::parseJson(value);
    return parsed.has_value() ? text::componentJson::deserialize(*parsed) : text::Component::text(std::string{value});
}

nbt::NbtList stringList(std::vector<std::string> values) {
    nbt::NbtList list{nbt::NbtType::String};
    for (std::string& one : values) {
        list.push(nbt::NbtTag::ofString(std::move(one)));
    }
    return list;
}

void putEnchantments(nbt::NbtCompound& tag, const ItemComponent::Enchantments& enchantments, std::int32_t farEnd,
                     std::span<const std::string> names) {
    const bool numbered = farEnd < versions::kV1_13;
    nbt::NbtList list{nbt::NbtType::Compound};
    for (const auto& [id, level] : enchantments.levels) {
        if (id < 0 || static_cast<std::size_t>(id) >= names.size()) {
            continue;
        }
        const std::string_view name = olderName(names[static_cast<std::size_t>(id)]);
        nbt::NbtCompound entry;
        if (numbered) {
            const auto* known = std::ranges::find(kNumberedEnchantments, name, &NumberedEnchantment::name);
            if (known == kNumberedEnchantments.end()) {
                continue;
            }
            entry.putShort("id", known->id);
        } else {
            entry.putString("id", std::string{kNamespace}.append(name));
        }
        entry.putShort("lvl", static_cast<std::int16_t>(std::clamp(level, 0, 0x7FFF)));
        list.push(nbt::NbtTag::ofCompound(std::move(entry)));
    }
    const char* key = enchantments.stored ? "StoredEnchantments" : (numbered ? "ench" : "Enchantments");
    tag.set(key, nbt::NbtTag::ofList(std::move(list)));
}

nbt::NbtCompound& display(nbt::NbtCompound& tag) {
    if (tag.getCompound("display") == nullptr) {
        tag.set("display", nbt::NbtTag::ofCompound({}));
    }
    return *tag.find("display")->get<nbt::NbtCompound>();
}

/// The newest effect each era numbers, which is one past its id here.
std::int32_t newestEffect(std::int32_t farEnd) {
    if (farEnd < versions::kV1_9) {
        return 23;
    }
    if (farEnd < versions::kV1_13) {
        return 27;
    }
    if (farEnd < versions::kV1_14) {
        return 30;
    }
    return farEnd < versions::kV1_19 ? 32 : 33;
}

void putPotion(nbt::NbtCompound& tag, io::ByteBuffer payload, std::int32_t farEnd) {
    if (protocol::readBoolean(payload)) {
        const std::int32_t potion = protocol::varInt::read(payload);
        if (potion >= 0 && static_cast<std::size_t>(potion) < kPotions.size() && farEnd >= versions::kV1_9) {
            tag.putString("Potion", std::string{kNamespace}.append(kPotions[static_cast<std::size_t>(potion)]));
        }
    }
    if (protocol::readBoolean(payload)) {
        tag.putInt("CustomPotionColor", payload.readInt());
    }
    nbt::NbtList effects{nbt::NbtType::Compound};
    const std::int32_t count = protocol::readCollectionSize(payload);
    for (std::int32_t i = 0; i < count && !payload.failed(); ++i) {
        const std::int32_t id = protocol::varInt::read(payload) + 1;
        const std::int32_t amplifier = protocol::varInt::read(payload);
        const std::int32_t duration = protocol::varInt::read(payload);
        const bool ambient = protocol::readBoolean(payload);
        const bool particles = protocol::readBoolean(payload);
        const bool icon = protocol::readBoolean(payload);
        for (std::size_t depth = 0; protocol::readBoolean(payload) && depth < 64 && !payload.failed(); ++depth) {
            for (int field = 0; field < 2; ++field) {
                static_cast<void>(protocol::varInt::read(payload));
            }
            payload.skip(3);
        }
        if (payload.failed() || id > newestEffect(farEnd)) {
            continue;
        }
        nbt::NbtCompound effect;
        effect.putByte("Id", static_cast<std::int8_t>(id));
        effect.putByte("Amplifier", static_cast<std::int8_t>(amplifier));
        effect.putInt("Duration", duration);
        effect.putBoolean("Ambient", ambient);
        effect.putBoolean("ShowParticles", particles);
        effect.putBoolean("ShowIcon", icon);
        effects.push(nbt::NbtTag::ofCompound(std::move(effect)));
    }
    if (!effects.empty() && !payload.failed()) {
        tag.set("CustomPotionEffects", nbt::NbtTag::ofList(std::move(effects)));
    }
}

void putWrittenBook(nbt::NbtCompound& tag, io::ByteBuffer payload) {
    std::string title = protocol::readString(payload);
    if (protocol::readBoolean(payload)) {
        static_cast<void>(protocol::readString(payload));
    }
    std::string author = protocol::readString(payload);
    const std::int32_t generation = protocol::varInt::read(payload);
    std::vector<std::string> pages;
    const std::int32_t count = protocol::readCollectionSize(payload);
    for (std::int32_t i = 0; i < count && !payload.failed(); ++i) {
        const text::Component page = text::componentNbt::deserialize(protocol::readAnonymousNbt(payload));
        if (protocol::readBoolean(payload)) {
            static_cast<void>(protocol::readAnonymousNbt(payload));
        }
        pages.push_back(nameText(page, true));
    }
    const bool resolved = protocol::readBoolean(payload);
    if (payload.failed()) {
        return;
    }
    tag.putString("title", std::move(title));
    tag.putString("author", std::move(author));
    tag.putInt("generation", generation);
    tag.putBoolean("resolved", resolved);
    tag.set("pages", nbt::NbtTag::ofList(stringList(std::move(pages))));
}

void putWritableBook(nbt::NbtCompound& tag, io::ByteBuffer payload) {
    std::vector<std::string> pages;
    const std::int32_t count = protocol::readCollectionSize(payload);
    for (std::int32_t i = 0; i < count && !payload.failed(); ++i) {
        pages.push_back(protocol::readString(payload));
        if (protocol::readBoolean(payload)) {
            static_cast<void>(protocol::readString(payload));
        }
    }
    if (!payload.failed()) {
        tag.set("pages", nbt::NbtTag::ofList(stringList(std::move(pages))));
    }
}

// parity: ItemStack.TooltipPart, the bits HideFlags gave each hidden part.
std::int32_t hideFlags(io::ByteBuffer payload) {
    static_cast<void>(protocol::readBoolean(payload));
    std::int32_t flags = 0;
    const std::int32_t count = protocol::readCollectionSize(payload);
    for (std::int32_t i = 0; i < count && !payload.failed(); ++i) {
        switch (protocol::varInt::read(payload)) {
        case component::kEnchantments:
            flags |= 1;
            break;
        case kAttributeModifiers:
            flags |= 2;
            break;
        case component::kUnbreakable:
            flags |= 4;
            break;
        case kCanBreak:
            flags |= 8;
            break;
        case kCanPlaceOn:
            flags |= 16;
            break;
        case component::kStoredEnchantments:
        case kPotionContents:
        case kWrittenBook:
            flags |= 32;
            break;
        case component::kDyedColor:
            flags |= 64;
            break;
        default:
            break;
        }
    }
    return payload.failed() ? 0 : flags;
}

io::ByteBuffer rawOf(const ItemComponent& source) {
    const auto* raw = source.as<ItemComponent::Raw>();
    return raw != nullptr ? io::ByteBuffer::wrap(raw->payload) : io::ByteBuffer{};
}

std::optional<std::int32_t> enchantmentId(const nbt::NbtTag* id, std::span<const std::string> names) {
    std::string_view name;
    if (const auto* text = id != nullptr ? id->get<std::string>() : nullptr) {
        name = bare(*text);
    } else if (const auto number = numberOf(id)) {
        const auto* known = std::ranges::find(kNumberedEnchantments, *number, &NumberedEnchantment::id);
        if (known == kNumberedEnchantments.end()) {
            return std::nullopt;
        }
        name = known->name;
    }
    if (name == kSweeping) {
        name = kSweepingEdge;
    }
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (!name.empty() && bare(names[index]) == name) {
            return static_cast<std::int32_t>(index);
        }
    }
    return std::nullopt;
}

std::optional<ItemComponent> enchantmentsFrom(const nbt::NbtList* list, bool stored,
                                              std::span<const std::string> names) {
    if (list == nullptr) {
        return std::nullopt;
    }
    ItemComponent::Enchantments enchantments{.stored = stored};
    for (const nbt::NbtTag& entry : *list) {
        const auto* compound = entry.get<nbt::NbtCompound>();
        if (compound == nullptr) {
            continue;
        }
        const auto id = enchantmentId(compound->find("id"), names);
        const auto level = numberOf(compound->find("lvl"));
        if (id.has_value() && level.has_value() && *level > 0) {
            enchantments.levels.emplace_back(*id, *level);
        }
    }
    return ItemComponent{stored ? component::kStoredEnchantments : component::kEnchantments, std::move(enchantments)};
}

/// A book's pages: a written one's are json and come with a title, a writable one's are plain.
std::optional<ItemComponent> bookFrom(const nbt::NbtCompound& tag) {
    const nbt::NbtList* pages = tag.getList("pages");
    if (pages == nullptr) {
        return std::nullopt;
    }
    const bool written = tag.contains("title");
    io::ByteBuffer payload;
    if (written) {
        protocol::writeString(payload, tag.getString("title"));
        protocol::writeBoolean(payload, false);
        protocol::writeString(payload, tag.getString("author"));
        protocol::varInt::write(payload, tag.getInt("generation"));
    }
    protocol::varInt::write(payload, static_cast<std::int32_t>(pages->size()));
    for (const nbt::NbtTag& page : *pages) {
        const auto* value = page.get<std::string>();
        const std::string_view line = value != nullptr ? std::string_view{*value} : std::string_view{};
        if (written) {
            protocol::writeAnonymousNbt(payload, text::componentNbt::serialize(fromText(line, true)));
        } else {
            protocol::writeString(payload, line);
        }
        protocol::writeBoolean(payload, false);
    }
    if (written) {
        protocol::writeBoolean(payload, tag.getBoolean("resolved"));
    }
    return ItemComponent{written ? kWrittenBook : kWritableBook, ItemComponent::Raw{payload.toVector()}};
}

} // namespace

nbt::NbtCompound toTag(const WireItemStack& item, std::int32_t farEnd, std::span<const std::string> enchantments) {
    nbt::NbtCompound tag;
    for (const ItemComponent& part : item.components) {
        if (part.id() == component::kCustomData) {
            if (const auto* custom = part.as<ItemComponent::CustomData>()) {
                tag = custom->tag;
            }
        }
    }
    const bool hasName = std::ranges::any_of(
        item.components, [](const ItemComponent& part) { return part.id() == component::kCustomName; });
    for (const ItemComponent& part : item.components) {
        switch (part.id()) {
        case component::kDamage:
            if (const auto* number = part.as<ItemComponent::Number>();
                number != nullptr && farEnd >= versions::kV1_13) {
                tag.putInt("Damage", number->value);
            }
            break;
        case component::kUnbreakable:
            tag.putBoolean("Unbreakable", true);
            break;
        case component::kCustomName:
        case component::kItemName:
            if (const auto* name = part.as<ItemComponent::Text>();
                name != nullptr && (part.id() == component::kCustomName || !hasName)) {
                const text::Component shown = part.id() == component::kItemName ? name->text.italic(false) : name->text;
                display(tag).putString("Name", nameText(shown, farEnd >= versions::kV1_13));
            }
            break;
        case component::kLore:
            if (const auto* lore = part.as<ItemComponent::Lore>()) {
                std::vector<std::string> lines;
                lines.reserve(lore->lines.size());
                for (const text::Component& line : lore->lines) {
                    lines.push_back(nameText(line, farEnd >= versions::kV1_14));
                }
                display(tag).set("Lore", nbt::NbtTag::ofList(stringList(std::move(lines))));
            }
            break;
        case component::kEnchantments:
        case component::kStoredEnchantments:
            if (const auto* levels = part.as<ItemComponent::Enchantments>()) {
                putEnchantments(tag, *levels, farEnd, enchantments);
            }
            break;
        case component::kDyedColor:
            if (const auto* color = part.as<ItemComponent::Color>()) {
                display(tag).putInt("color", color->rgb);
            }
            break;
        case component::kRepairCost:
            if (const auto* cost = part.as<ItemComponent::Number>()) {
                tag.putInt("RepairCost", cost->value);
            }
            break;
        case kTooltipDisplay:
            if (const std::int32_t flags = hideFlags(rawOf(part)); flags != 0) {
                tag.putInt("HideFlags", flags);
            }
            break;
        case kPotionContents:
            putPotion(tag, rawOf(part), farEnd);
            break;
        case kWrittenBook:
            putWrittenBook(tag, rawOf(part));
            break;
        case kWritableBook:
            putWritableBook(tag, rawOf(part));
            break;
        default:
            break;
        }
    }
    return tag;
}

std::vector<ItemComponent> toComponents(const nbt::NbtCompound& tag, std::int32_t farEnd,
                                        std::span<const std::string> enchantments) {
    std::vector<ItemComponent> components;
    nbt::NbtCompound rest = tag;
    if (const nbt::NbtCompound* shown = tag.getCompound("display")) {
        if (const auto* name = shown->find("Name"); name != nullptr && name->get<std::string>() != nullptr) {
            components.emplace_back(component::kCustomName, ItemComponent::Text{fromText(*name->get<std::string>(),
                                                                                         farEnd >= versions::kV1_13)});
        }
        if (const nbt::NbtList* lore = shown->getList("Lore")) {
            ItemComponent::Lore lines;
            for (const nbt::NbtTag& line : *lore) {
                if (const auto* value = line.get<std::string>()) {
                    lines.lines.push_back(fromText(*value, farEnd >= versions::kV1_14));
                }
            }
            components.emplace_back(component::kLore, std::move(lines));
        }
        if (const auto color = numberOf(shown->find("color"))) {
            components.emplace_back(component::kDyedColor, ItemComponent::Color{*color});
        }
        rest.remove("display");
    }
    if (farEnd >= versions::kV1_13) {
        if (const auto damage = numberOf(tag.find("Damage")); damage.has_value() && *damage > 0) {
            components.emplace_back(component::kDamage, ItemComponent::Number{*damage});
        }
    }
    if (tag.getBoolean("Unbreakable")) {
        components.emplace_back(component::kUnbreakable, ItemComponent::Unit{});
    }
    if (const auto cost = numberOf(tag.find("RepairCost")); cost.has_value() && *cost > 0) {
        components.emplace_back(component::kRepairCost, ItemComponent::Number{*cost});
    }
    if (auto own =
            enchantmentsFrom(tag.getList(farEnd < versions::kV1_13 ? "ench" : "Enchantments"), false, enchantments)) {
        components.push_back(*std::move(own));
    }
    if (auto stored = enchantmentsFrom(tag.getList("StoredEnchantments"), true, enchantments)) {
        components.push_back(*std::move(stored));
    }
    const std::string_view potion = bare(tag.getString("Potion"));
    const auto* known = std::ranges::find(kPotions, potion);
    const auto color = numberOf(tag.find("CustomPotionColor"));
    if (known != kPotions.end() || color.has_value()) {
        io::ByteBuffer payload;
        protocol::writeBoolean(payload, known != kPotions.end());
        if (known != kPotions.end()) {
            protocol::varInt::write(payload, static_cast<std::int32_t>(known - kPotions.begin()));
        }
        protocol::writeBoolean(payload, color.has_value());
        if (color.has_value()) {
            payload.writeInt(*color);
        }
        protocol::varInt::write(payload, 0);
        protocol::writeBoolean(payload, false);
        components.emplace_back(kPotionContents, ItemComponent::Raw{payload.toVector()});
    }
    if (auto book = bookFrom(tag)) {
        components.push_back(*std::move(book));
    }
    for (const std::string_view handled : {"Damage", "Unbreakable", "RepairCost", "ench", "Enchantments",
                                           "StoredEnchantments", "Potion", "CustomPotionColor", "CustomPotionEffects",
                                           "HideFlags", "pages", "title", "author", "generation", "resolved"}) {
        rest.remove(handled);
    }
    if (!rest.empty()) {
        components.emplace_back(component::kCustomData, ItemComponent::CustomData{std::move(rest)});
    }
    return components;
}

namespace {

std::int32_t potionDamage(const WireItemStack& item, bool splash) {
    std::int32_t damage = 0;
    for (const ItemComponent& part : item.components) {
        if (part.id() != kPotionContents) {
            continue;
        }
        io::ByteBuffer payload = rawOf(part);
        if (protocol::readBoolean(payload)) {
            const std::int32_t potion = protocol::varInt::read(payload);
            if (!payload.failed() && potion >= 0 && static_cast<std::size_t>(potion) < kPotionDamage.size()) {
                damage = kPotionDamage[static_cast<std::size_t>(potion)];
            }
        }
    }
    return splash ? damage + kSplashBit : damage;
}

} // namespace

bool isPotion(std::int32_t nativeId) noexcept {
    return nativeId == kPotion || nativeId == kSplashPotion || nativeId == kLingeringPotion;
}

void fromStack(WireItemStack& item, std::int32_t data, std::int32_t farEnd, std::span<const std::string> enchantments) {
    nbt::NbtCompound tag;
    for (const ItemComponent& part : item.components) {
        if (const auto* custom = part.as<ItemComponent::CustomData>()) {
            tag = custom->tag;
        }
    }
    item.components = toComponents(tag, farEnd, enchantments);
    item.removedComponents.clear();
    if (data == 0) {
        return;
    }
    if (farEnd < versions::kV1_9 && item.itemId == kPotion) {
        const bool splash = data >= 2 * kSplashBit;
        const std::int32_t base = splash ? data - kSplashBit : data;
        const auto* known = std::ranges::find(kPotionDamage, static_cast<std::int16_t>(base));
        if (splash) {
            item.itemId = kSplashPotion;
        }
        if (known != kPotionDamage.end()) {
            io::ByteBuffer payload;
            protocol::writeBoolean(payload, true);
            protocol::varInt::write(payload, static_cast<std::int32_t>(known - kPotionDamage.begin()));
            const auto color = numberOf(tag.find("CustomPotionColor"));
            protocol::writeBoolean(payload, color.has_value());
            if (color.has_value()) {
                payload.writeInt(*color);
            }
            protocol::varInt::write(payload, 0);
            protocol::writeBoolean(payload, false);
            std::erase_if(item.components, [](const ItemComponent& part) { return part.id() == kPotionContents; });
            item.components.emplace_back(kPotionContents, ItemComponent::Raw{payload.toVector()});
        }
        return;
    }
    item.components.emplace_back(component::kDamage, ItemComponent::Number{data});
}

WireItemStack toStack(WireItemStack item, std::int32_t nativeId, std::int32_t farEnd,
                      std::span<const std::string> enchantments) {
    constexpr int kDataShift = 16;
    constexpr std::int32_t kIdMask = 0xFFFF;
    nbt::NbtCompound tag = toTag(item, farEnd, enchantments);
    // Below 1.13 the data value beside the id carried both the variant and the damage.
    std::int32_t data = 0;
    if (farEnd < versions::kV1_9 && isPotion(nativeId)) {
        data = potionDamage(item, nativeId != kPotion);
    } else if (farEnd < versions::kV1_13 && (item.itemId >> kDataShift) == 0) {
        for (const ItemComponent& part : item.components) {
            if (const auto* damage = part.as<ItemComponent::Number>();
                damage != nullptr && part.id() == component::kDamage) {
                data = damage->value & kIdMask;
            }
        }
    }
    if (data != 0) {
        item.itemId = (item.itemId & kIdMask) | (data << kDataShift);
    }
    item.components.clear();
    item.removedComponents.clear();
    if (!tag.empty()) {
        item.components.emplace_back(component::kCustomData, ItemComponent::CustomData{std::move(tag)});
    }
    return item;
}

std::optional<std::vector<std::string>> enchantmentNames(const std::vector<std::uint8_t>& body) {
    io::ByteBuffer input = io::ByteBuffer::wrap(body);
    if (protocol::readString(input) != "minecraft:enchantment") {
        return std::nullopt;
    }
    io::ByteBuffer whole = io::ByteBuffer::wrap(body);
    const auto registry = protocol::configuration::RegistryDataPacket::read(whole);
    if (!registry.has_value()) {
        return std::nullopt;
    }
    std::vector<std::string> names;
    names.reserve(registry->entries.size());
    for (const protocol::configuration::RegistryEntry& entry : registry->entries) {
        names.push_back(entry.key);
    }
    return names;
}

} // namespace basalt::wire::multiversion::java::legacyItemTags
