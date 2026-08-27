#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/java/Stack765.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion::java::stack765 {
namespace {

namespace pv = protocol;

constexpr std::int32_t kDamage = 3;
constexpr std::int32_t kUnbreakable = 4;
constexpr std::int32_t kCustomName = 5;
constexpr std::int32_t kLore = 7;
constexpr std::int32_t kEnchantments = 9;
constexpr std::int32_t kRepairCost = 16;
constexpr std::int32_t kStoredEnchantments = 23;

constexpr std::array<std::string_view, 42> kEnchantmentNames = {"protection",
                                                                "fire_protection",
                                                                "feather_falling",
                                                                "blast_protection",
                                                                "projectile_protection",
                                                                "respiration",
                                                                "aqua_affinity",
                                                                "thorns",
                                                                "depth_strider",
                                                                "frost_walker",
                                                                "binding_curse",
                                                                "soul_speed",
                                                                "swift_sneak",
                                                                "sharpness",
                                                                "smite",
                                                                "bane_of_arthropods",
                                                                "knockback",
                                                                "fire_aspect",
                                                                "looting",
                                                                "sweeping_edge",
                                                                "efficiency",
                                                                "silk_touch",
                                                                "unbreaking",
                                                                "fortune",
                                                                "power",
                                                                "punch",
                                                                "flame",
                                                                "infinity",
                                                                "luck_of_the_sea",
                                                                "lure",
                                                                "loyalty",
                                                                "impaling",
                                                                "riptide",
                                                                "channeling",
                                                                "multishot",
                                                                "quick_charge",
                                                                "piercing",
                                                                "density",
                                                                "breach",
                                                                "wind_burst",
                                                                "mending",
                                                                "vanishing_curse"};

nbt::NbtTag textFromJson(std::string_view json) {
    auto parsed = common::parseJson(json);
    if (!parsed.has_value()) {
        return text::componentNbt::serialize(text::Component{});
    }
    return text::componentNbt::serialize(text::componentJson::deserialize(*parsed));
}

using Component = std::pair<std::int32_t, std::vector<std::uint8_t>>;

void enchantments(const nbt::NbtList* list, std::int32_t componentId, std::vector<Component>& out) {
    if (list == nullptr || list->elementType() != nbt::NbtType::Compound) {
        return;
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> levels;
    for (const nbt::NbtTag& each : list->elements()) {
        const auto* compound = each.get<nbt::NbtCompound>();
        const nbt::NbtTag* id = compound != nullptr ? compound->find("id") : nullptr;
        const std::string* name = id != nullptr ? id->get<std::string>() : nullptr;
        if (name == nullptr) {
            continue;
        }
        std::string_view bare = *name;
        bare.remove_prefix(std::min(bare.size(), bare.starts_with("minecraft:") ? std::size_t{10} : std::size_t{0}));
        const auto found = std::ranges::find(kEnchantmentNames, bare);
        if (found == kEnchantmentNames.end()) {
            continue;
        }
        levels.emplace_back(static_cast<std::int32_t>(found - kEnchantmentNames.begin()), compound->getShort("lvl", 1));
    }
    if (levels.empty()) {
        return;
    }
    io::ByteBuffer payload;
    pv::varInt::write(payload, static_cast<std::int32_t>(levels.size()));
    for (const auto& [id, level] : levels) {
        pv::varInt::write(payload, id);
        pv::varInt::write(payload, level);
    }
    pv::writeBoolean(payload, true);
    out.emplace_back(componentId, payload.toVector());
}

void display(const nbt::NbtCompound& display, std::vector<Component>& out) {
    if (const nbt::NbtTag* name = display.find("Name")) {
        if (const std::string* json = name->get<std::string>()) {
            io::ByteBuffer payload;
            pv::writeAnonymousNbt(payload, textFromJson(*json));
            out.emplace_back(kCustomName, payload.toVector());
        }
    }
    if (const nbt::NbtList* lore = display.getList("Lore");
        lore != nullptr && lore->elementType() == nbt::NbtType::String) {
        io::ByteBuffer payload;
        pv::varInt::write(payload, static_cast<std::int32_t>(lore->size()));
        for (const nbt::NbtTag& line : lore->elements()) {
            const std::string* json = line.get<std::string>();
            pv::writeAnonymousNbt(payload, textFromJson(json == nullptr ? std::string_view{} : *json));
        }
        out.emplace_back(kLore, payload.toVector());
    }
}

} // namespace

bool toComponents(io::ByteBuffer& in, io::ByteBuffer& out) {
    if (!pv::readBoolean(in)) {
        pv::varInt::write(out, 0);
        return !in.failed();
    }
    const std::int32_t id = pv::varInt::read(in);
    const std::int32_t count = static_cast<std::int8_t>(in.readByte());
    nbt::NbtCompound tag;
    if (!in.readable().empty() && in.readable().front() == 0) {
        in.skip(1);
    } else {
        const nbt::NbtTag parsed = pv::readAnonymousNbt(in);
        if (const auto* compound = parsed.get<nbt::NbtCompound>()) {
            tag = *compound;
        }
    }
    if (in.failed()) {
        return false;
    }
    std::vector<Component> components;
    if (tag.contains("Damage")) {
        io::ByteBuffer payload;
        pv::varInt::write(payload, tag.getInt("Damage", 0));
        components.emplace_back(kDamage, payload.toVector());
    }
    if (tag.getBoolean("Unbreakable")) {
        components.emplace_back(kUnbreakable, std::vector<std::uint8_t>{1});
    }
    if (const nbt::NbtTag* shown = tag.find("display")) {
        if (const auto* compound = shown->get<nbt::NbtCompound>()) {
            display(*compound, components);
        }
    }
    enchantments(tag.getList("Enchantments"), kEnchantments, components);
    if (tag.contains("RepairCost")) {
        io::ByteBuffer payload;
        pv::varInt::write(payload, tag.getInt("RepairCost", 0));
        components.emplace_back(kRepairCost, payload.toVector());
    }
    enchantments(tag.getList("StoredEnchantments"), kStoredEnchantments, components);
    std::ranges::sort(components, {}, &Component::first);
    pv::varInt::write(out, count);
    pv::varInt::write(out, id);
    pv::varInt::write(out, static_cast<std::int32_t>(components.size()));
    pv::varInt::write(out, 0);
    for (const auto& [componentId, payload] : components) {
        pv::varInt::write(out, componentId);
        out.writeBytes(payload);
    }
    return true;
}

} // namespace basalt::wire::multiversion::java::stack765
