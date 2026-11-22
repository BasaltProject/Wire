#include "HostHud.hpp"
#include "Scenario.hpp"

#include <algorithm>
#include <array>
#include <charconv>

namespace basalt::wire::wasm::trace {
namespace hud {
namespace {
constexpr std::string_view kWorldName = "world";
constexpr std::array<std::string_view, 3> kItems{"minecraft:diamond_sword", "minecraft:stone", "minecraft:iron_helmet"};
constexpr std::int32_t kMaxSlot = 40;

template <typename T>
[[nodiscard]] std::int32_t findIn(std::vector<T>& list, std::int32_t handle, T*& out) {
    if (handle < 1 || static_cast<std::size_t>(handle) > list.size()) {
        return kErrInvalid;
    }
    T& item = list[static_cast<std::size_t>(handle - 1)];
    if (!item.alive) {
        return kErrStale;
    }
    out = &item;
    return kOk;
}

struct Point {
    std::optional<double> x;
    std::optional<double> y;
    std::optional<double> z;

    [[nodiscard]] bool all() const { return x && y && z; }

    [[nodiscard]] bool any() const { return x || y || z; }
};

[[nodiscard]] std::int32_t takePoint(const Value& spec, Point& point) {
    std::int32_t status = takeNumber(spec, "x", point.x);
    status = status != kOk ? status : takeNumber(spec, "y", point.y);
    return status != kOk ? status : takeNumber(spec, "z", point.z);
}

[[nodiscard]] Value audienceValue(const std::optional<std::vector<std::string>>& audience) {
    return audience ? stringList(*audience) : Value{};
}

[[nodiscard]] std::int32_t setAudience(World& world, const Value& spec, std::optional<std::vector<std::string>>& out) {
    const Value* audience = spec.find("audience");
    if (audience == nullptr) {
        return kOk;
    }
    if (audience->isNull()) {
        out.reset();
        return kOk;
    }
    const std::optional<std::vector<std::string>> uuids = readAudience(world, *audience);
    if (!uuids) {
        return kErrInvalid;
    }
    out = *uuids;
    return kOk;
}

void hologramCreate(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    const GuestMemory memory = c.memory();
    const auto worldName = memory.string(c.i32(0), c.i32(1));
    const auto text = memory.string(c.i32(2), c.i32(3));
    Rec rec("hud_hologram_create");
    rec.str(worldName).str(text).f64(c.f64(4)).f64(c.f64(5)).f64(c.f64(6));
    if (!worldName || !text) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    std::string rendered;
    if (*worldName != kWorldName || renderPlain(world, *text, rendered) != kOk) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    state.holograms.push_back(
        Hologram{.entity = state.nextEntity++, .text = rendered, .x = c.f64(4), .y = c.f64(5), .z = c.f64(6)});
    c.ret(rec.done(static_cast<std::int32_t>(state.holograms.size())));
}

void hologramSet(void* u, Call& c) {
    World& world = worldOf(u);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    Rec rec("hud_hologram_set");
    rec.i(c.i32(0)).value(spec);
    Hologram* hologram = nullptr;
    std::int32_t status = findIn(stateOf(world).holograms, c.i32(0), hologram);
    if (status == kOk && !spec) {
        status = kErrInvalid;
    }
    Point point;
    if (status == kOk) {
        status = takePoint(*spec, point);
        status = status != kOk || !point.any() || point.all() ? status : kErrInvalid;
    }
    std::optional<std::string> text;
    if (status == kOk) {
        status = takeText(world, *spec, "text", text);
        status = status == kErrAbsent ? kErrInvalid : status;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    if (text) {
        hologram->text = *text;
    }
    if (point.all()) {
        hologram->x = *point.x;
        hologram->y = *point.y;
        hologram->z = *point.z;
    }
    c.ret(rec.done(setAudience(world, *spec, hologram->audience)));
}

void hologramGet(void* u, Call& c) {
    Rec rec("hud_hologram_get");
    rec.i(c.i32(0)).cap(c.i32(2));
    Hologram* hologram = nullptr;
    const std::int32_t status = findIn(stateOf(worldOf(u)).holograms, c.i32(0), hologram);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    Value::Map map;
    map.push_back({"id", Value{hologram->entity}});
    map.push_back({"world", Value{std::string{kWorldName}}});
    map.push_back({"x", Value{hologram->x}});
    map.push_back({"y", Value{hologram->y}});
    map.push_back({"z", Value{hologram->z}});
    map.push_back({"alive", Value{true}});
    map.push_back({"text", Value{hologram->text}});
    map.push_back({"audience", audienceValue(hologram->audience)});
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2), Value{std::move(map)}));
}

void hologramRemove(void* u, Call& c) {
    Hologram* hologram = nullptr;
    const std::int32_t status = findIn(stateOf(worldOf(u)).holograms, c.i32(0), hologram);
    if (status == kOk) {
        hologram->alive = false;
    }
    c.ret(Rec("hud_hologram_remove").i(c.i32(0)).done(status));
}

[[nodiscard]] std::string attributeName(std::string_view name) {
    return name.find(':') == std::string_view::npos ? "minecraft:" + std::string{name} : std::string{name};
}

[[nodiscard]] std::int32_t applySkin(const Value& skin) {
    if (skin.asMap() == nullptr) {
        return kErrInvalid;
    }
    std::optional<std::string> texture;
    std::optional<std::string> signature;
    std::optional<std::string> player;
    std::int32_t status = takeString(skin, "texture", texture);
    status = status != kOk ? status : takeString(skin, "signature", signature);
    status = status != kOk ? status : takeString(skin, "name", player);
    if (status != kOk) {
        return status;
    }
    if (player) {
        return kOk;
    }
    return texture ? kOk : kErrInvalid;
}

[[nodiscard]] std::int32_t applyEquipment(const Value& equipment, Npc& npc) {
    const Value::Map* slots = equipment.asMap();
    if (slots == nullptr) {
        return kErrInvalid;
    }
    for (const MapEntry& slotEntry : *slots) {
        std::int32_t slot = -1;
        const auto [end, error] =
            std::from_chars(slotEntry.key.data(), slotEntry.key.data() + slotEntry.key.size(), slot);
        if (error != std::errc{} || end != slotEntry.key.data() + slotEntry.key.size() || slot < 0 || slot > kMaxSlot) {
            return kErrInvalid;
        }
        if (slotEntry.value.isNull()) {
            npc.items.erase(slot);
            continue;
        }
        std::optional<std::string> item;
        std::optional<std::int64_t> count;
        std::int32_t status = takeString(slotEntry.value, "item", item);
        status = status != kOk ? status : takeInt(slotEntry.value, "count", count);
        if (status != kOk || !item || std::ranges::find(kItems, *item) == kItems.end()) {
            return kErrInvalid;
        }
        npc.items[slot] = {*item, count.value_or(1)};
    }
    return kOk;
}

[[nodiscard]] std::int32_t applyAttributes(const Value& attributes, Npc& npc) {
    const Value::Map* map = attributes.asMap();
    if (map == nullptr) {
        return kErrInvalid;
    }
    for (const MapEntry& entry : *map) {
        const std::string name = attributeName(entry.key);
        const double* real = entry.value.asFloat();
        const std::int64_t* whole = entry.value.asInt();
        if ((real == nullptr && whole == nullptr) || !npc.attributes.contains(name)) {
            return kErrInvalid;
        }
        npc.attributes[name] = real != nullptr ? *real : static_cast<double>(*whole);
    }
    return kOk;
}

[[nodiscard]] std::int32_t applyNpcSpec(World& world, const Value& spec, Npc& npc) {
    std::optional<std::string> name;
    std::optional<double> yaw;
    std::optional<double> pitch;
    std::optional<bool> follows;
    std::optional<double> health;
    std::optional<std::int64_t> held;
    std::optional<std::int64_t> flags;
    std::optional<std::int64_t> pose;
    Point point;
    std::int32_t status = takeText(world, spec, "name", name);
    status = status != kOk ? status : takePoint(spec, point);
    status = status != kOk || !point.any() || point.all() ? status : kErrInvalid;
    status = status != kOk ? status : takeNumber(spec, "yaw", yaw);
    status = status != kOk ? status : takeNumber(spec, "pitch", pitch);
    status = status != kOk ? status : takeBool(spec, "looks_at_players", follows);
    status = status != kOk ? status : takeNumber(spec, "health", health);
    status = status != kOk ? status : takeInt(spec, "held_slot", held);
    status = status != kOk ? status : takeInt(spec, "flags", flags);
    status = status != kOk ? status : takeInt(spec, "pose", pose);
    if (status != kOk || (held && (*held < 0 || *held > 8))) {
        return status == kErrAbsent || status == kOk ? kErrInvalid : status;
    }
    Npc staged = npc;
    if (name) {
        staged.name = *name;
    }
    if (point.all()) {
        staged.x = *point.x;
        staged.y = *point.y;
        staged.z = *point.z;
    }
    if (const Value* skin = spec.find("skin"); skin != nullptr) {
        status = applySkin(*skin);
    }
    status = status != kOk ? status : setAudience(world, spec, staged.audience);
    if (status != kOk) {
        npc = std::move(staged);
        return status;
    }
    staged.health = health.value_or(staged.health);
    staged.held = held.value_or(staged.held);
    staged.flags = flags.value_or(staged.flags);
    staged.pose = pose.value_or(staged.pose);
    if (const Value* attributes = spec.find("attributes"); attributes != nullptr) {
        status = applyAttributes(*attributes, staged);
    }
    if (const Value* equipment = spec.find("equipment"); status == kOk && equipment != nullptr) {
        status = applyEquipment(*equipment, staged);
    }
    npc = std::move(staged);
    return status;
}

void npcCreate(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(0), c.i32(1));
    Rec rec("hud_npc_create");
    rec.value(spec).i(c.i32(2));
    if (!spec) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::int32_t token = c.i32(2);
    std::optional<std::string> name;
    std::optional<std::string> uuid;
    std::optional<std::string> worldName;
    Point point;
    std::int32_t status = takeText(world, *spec, "name", name);
    status = status != kOk ? status : takeString(*spec, "uuid", uuid);
    status = status != kOk ? status : takePoint(*spec, point);
    status = status != kOk ? status : takeString(*spec, "world", worldName);
    if (status != kOk || !point.all() || !name || name->empty() || (uuid && token != 0)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (worldName && *worldName != kWorldName) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    if (const Value* skin = spec->find("skin"); skin != nullptr && applySkin(*skin) != kOk) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    Npc npc;
    npc.entity = state.nextEntity++;
    npc.token = token;
    npc.uuid = uuid.value_or("npc-uuid-" + std::to_string(npc.entity));
    npc.name = *name;
    npc.x = *point.x;
    npc.y = *point.y;
    npc.z = *point.z;
    npc.attributes = {{"minecraft:max_health", 20.0}, {"minecraft:movement_speed", 0.1}};
    state.npcs.push_back(std::move(npc));
    c.ret(rec.done(static_cast<std::int32_t>(state.npcs.size())));
}

void npcSet(void* u, Call& c) {
    World& world = worldOf(u);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    Rec rec("hud_npc_set");
    rec.i(c.i32(0)).value(spec);
    Npc* npc = nullptr;
    std::int32_t status = findIn(stateOf(world).npcs, c.i32(0), npc);
    if (status == kOk) {
        status = spec ? applyNpcSpec(world, *spec, *npc) : kErrInvalid;
    }
    c.ret(rec.done(status));
}

void npcGet(void* u, Call& c) {
    Rec rec("hud_npc_get");
    rec.i(c.i32(0)).cap(c.i32(2));
    Npc* npc = nullptr;
    const std::int32_t status = findIn(stateOf(worldOf(u)).npcs, c.i32(0), npc);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    Value::Map map;
    map.push_back({"id", Value{npc->entity}});
    map.push_back({"uuid", Value{npc->uuid}});
    map.push_back({"name", Value{npc->name}});
    map.push_back({"world", Value{std::string{kWorldName}}});
    map.push_back({"x", Value{npc->x}});
    map.push_back({"y", Value{npc->y}});
    map.push_back({"z", Value{npc->z}});
    map.push_back({"alive", Value{true}});
    map.push_back({"audience", audienceValue(npc->audience)});
    map.push_back({"health", Value{npc->health}});
    map.push_back({"max_health", Value{npc->attributes.at("minecraft:max_health")}});
    map.push_back({"flags", Value{npc->flags}});
    map.push_back({"pose", Value{npc->pose}});
    map.push_back({"held_slot", Value{npc->held}});
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2), Value{std::move(map)}));
}

void npcItem(void* u, Call& c) {
    Rec rec("hud_npc_item");
    rec.i(c.i32(0)).i(c.i32(1)).cap(c.i32(3));
    Npc* npc = nullptr;
    std::int32_t status = findIn(stateOf(worldOf(u)).npcs, c.i32(0), npc);
    if (status == kOk && (c.i32(1) < 0 || c.i32(1) > kMaxSlot)) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const auto held = npc->items.find(c.i32(1));
    if (held == npc->items.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    Value::Map map;
    map.push_back({"item", Value{held->second.first}});
    map.push_back({"count", Value{held->second.second}});
    c.ret(doneValue(rec, c.memory(), c.i32(2), c.i32(3), Value{std::move(map)}));
}

void npcAttribute(void* u, Call& c) {
    const auto name = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("hud_npc_attribute");
    rec.i(c.i32(0)).str(name);
    Npc* npc = nullptr;
    std::int32_t status = findIn(stateOf(worldOf(u)).npcs, c.i32(0), npc);
    if (status == kOk && !name) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const auto found = npc->attributes.find(attributeName(*name));
    if (found == npc->attributes.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const bool wrote = c.memory().write(c.i32(3), found->second);
    c.ret(wrote ? rec.done(kOk, "out=" + number(found->second)) : rec.done(kErrInvalid));
}

void npcAnimate(void* u, Call& c) {
    Npc* npc = nullptr;
    const std::int32_t status = findIn(stateOf(worldOf(u)).npcs, c.i32(0), npc);
    c.ret(Rec("hud_npc_animate").i(c.i32(0)).i(c.i32(1)).done(status));
}

void npcRemove(void* u, Call& c) {
    Npc* npc = nullptr;
    const std::int32_t status = findIn(stateOf(worldOf(u)).npcs, c.i32(0), npc);
    if (status == kOk) {
        npc->alive = false;
    }
    c.ret(Rec("hud_npc_remove").i(c.i32(0)).done(status));
}

bool npcClick(World& world, std::string_view rest) {
    const std::size_t split = rest.find(' ');
    if (split == std::string_view::npos) {
        return false;
    }
    std::int32_t handle = 0;
    const std::string_view number = rest.substr(0, split);
    const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), handle);
    Npc* npc = nullptr;
    const Player* player = world.playerByName(rest.substr(split + 1));
    if (error != std::errc{} || end != number.data() + number.size() ||
        findIn(stateOf(world).npcs, handle, npc) != kOk || player == nullptr || !player->online || npc->token == 0) {
        return false;
    }
    Value::Map map;
    map.push_back({"npc", Value{handle}});
    map.push_back({"player", Value{player->name}});
    map.push_back({"uuid", Value{uuidOf(player->name)}});
    return deliverValue(world, npc->token, Value{std::move(map)}) == kOk;
}

const ScenarioVerbRegistrar kNpcClickVerb{"npc-click", npcClick};
} // namespace
} // namespace hud

void addHudActorsImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"hud_hologram_create", hud::hologramCreate, user},
                              {"hud_hologram_set", hud::hologramSet, user},
                              {"hud_hologram_get", hud::hologramGet, user},
                              {"hud_hologram_remove", hud::hologramRemove, user},
                              {"hud_npc_create", hud::npcCreate, user},
                              {"hud_npc_set", hud::npcSet, user},
                              {"hud_npc_get", hud::npcGet, user},
                              {"hud_npc_item", hud::npcItem, user},
                              {"hud_npc_attribute", hud::npcAttribute, user},
                              {"hud_npc_animate", hud::npcAnimate, user},
                              {"hud_npc_remove", hud::npcRemove, user},
                          });
}

} // namespace basalt::wire::wasm::trace
