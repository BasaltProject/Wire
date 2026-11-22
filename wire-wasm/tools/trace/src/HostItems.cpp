#include "HostItems.hpp"

#include "HostEntitiesState.hpp"

#include <algorithm>
#include <cmath>

namespace basalt::wire::wasm::trace::items {
namespace {
constexpr std::int32_t kPlayerSlots = 41;
constexpr std::int32_t kEnderSlots = 27;
constexpr std::int32_t kWindowPlayerSlots = 36;
constexpr std::int32_t kHotbarStart = 27;
constexpr std::int32_t kStorageEnd = 36;

const char* const kKnownItems[] = {
    "minecraft:stone",      "minecraft:diamond",    "minecraft:diamond_sword", "minecraft:stick",
    "minecraft:oak_planks", "minecraft:oak_log",    "minecraft:apple",         "minecraft:ender_pearl",
    "minecraft:bread",      "minecraft:iron_ingot", "minecraft:book",          "minecraft:emerald",
    "minecraft:barrier",    "minecraft:coal",       "minecraft:wheat",         "minecraft:rotten_flesh"};

[[nodiscard]] std::string qualified(std::string_view id) {
    return id.find(':') == std::string_view::npos ? "minecraft:" + std::string{id} : std::string{id};
}

[[nodiscard]] bool known(const std::string& id) {
    return std::any_of(std::begin(kKnownItems), std::end(kKnownItems), [&id](const char* item) { return id == item; });
}

void seed(State& state) {
    if (state.seeded) {
        return;
    }
    state.seeded = true;
    std::vector<Slot> chest(27);
    chest[3] = Item{.id = "minecraft:bread", .count = 5};
    state.blocks["world:10:64:10"] = std::move(chest);
    state.blocks["world:11:64:10"] = std::vector<Slot>(5);
    state.recipes = {"minecraft:oak_planks", "minecraft:stick"};
    state.recipeTypes = {{"minecraft:oak_planks", "minecraft:crafting_shapeless"},
                         {"minecraft:stick", "minecraft:crafting_shaped"}};
}

[[nodiscard]] std::vector<Slot>& playerSlots(State& state, const std::string& name) {
    std::vector<Slot>& slots = state.players[name];
    if (slots.empty()) {
        slots.resize(kPlayerSlots);
    }
    return slots;
}

[[nodiscard]] std::vector<Slot>& enderSlots(State& state, const std::string& name) {
    std::vector<Slot>& slots = state.ender[name];
    if (slots.empty()) {
        slots.resize(kEnderSlots);
    }
    return slots;
}

[[nodiscard]] std::string kindName(Kind kind) {
    switch (kind) {
    case Kind::Player:
        return "player";
    case Kind::Block:
        return "block";
    case Kind::Window:
        return "window";
    case Kind::Ender:
        return "ender";
    default:
        return "entity";
    }
}

struct Resolved {
    Inventory* inventory = nullptr;
    std::vector<Slot>* slots = nullptr;
    Slot* cursor = nullptr;
    Player* player = nullptr;
};

[[nodiscard]] std::int32_t resolve(World& world, std::int32_t handle, Resolved& out) {
    State& state = stateOf(world);
    if (handle <= 0 || static_cast<std::size_t>(handle) > state.handles.size()) {
        return kErrInvalid;
    }
    Inventory& inventory = state.handles[static_cast<std::size_t>(handle - 1)];
    if (inventory.released) {
        return kErrStale;
    }
    out.inventory = &inventory;
    if (inventory.kind == Kind::Block) {
        out.slots = &state.blocks[inventory.owner];
        return kOk;
    }
    if (inventory.kind == Kind::Entity) {
        out.slots = &state.blocks[inventory.owner];
        return kOk;
    }
    out.player = world.playerByName(inventory.owner);
    if (out.player == nullptr || !out.player->online) {
        return kErrStale;
    }
    if (inventory.kind == Kind::Player) {
        out.slots = &playerSlots(state, inventory.owner);
    } else if (inventory.kind == Kind::Ender) {
        out.slots = &enderSlots(state, inventory.owner);
    } else {
        const auto found = state.windows.find(inventory.owner);
        if (found == state.windows.end() || found->second.container != inventory.container) {
            return kErrStale;
        }
        out.slots = &found->second.slots;
        out.cursor = &found->second.cursor;
    }
    return kOk;
}

[[nodiscard]] std::string where(const Inventory& inventory) { return kindName(inventory.kind) + ":" + inventory.owner; }

[[nodiscard]] std::int32_t newHandle(World& world, Kind kind, std::string owner, std::int32_t container = 0) {
    State& state = stateOf(world);
    state.handles.push_back(Inventory{kind, std::move(owner), container, false});
    return static_cast<std::int32_t>(state.handles.size());
}

[[nodiscard]] std::int32_t ownedHandle(World& world, std::int32_t playerHandle, Kind kind) {
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(world, playerHandle, player);
    if (status != kOk) {
        return status;
    }
    return newHandle(world, kind, player->name);
}

void ofPlayer(void* u, Call& c) {
    World& world = worldOf(u);
    seed(stateOf(world));
    Rec rec("inventory_of_player");
    rec.i(c.i32(0));
    c.ret(rec.done(ownedHandle(world, c.i32(0), Kind::Player)));
}

void ofEnder(void* u, Call& c) {
    World& world = worldOf(u);
    seed(stateOf(world));
    Rec rec("inventory_of_ender");
    rec.i(c.i32(0));
    c.ret(rec.done(ownedHandle(world, c.i32(0), Kind::Ender)));
}

void ofBlock(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    seed(state);
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("inventory_of_block");
    rec.str(name).i(c.i32(2)).i(c.i32(3)).i(c.i32(4));
    if (!name) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::string key = (name->empty() ? "world" : std::string{*name}) + ":" + std::to_string(c.i32(2)) + ":" +
                            std::to_string(c.i32(3)) + ":" + std::to_string(c.i32(4));
    if (!state.blocks.contains(key)) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    c.ret(rec.done(newHandle(world, Kind::Block, key)));
}

void ofWindow(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    Player* player = nullptr;
    Rec rec("inventory_of_window");
    rec.i(c.i32(0));
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const auto found = state.windows.find(player->name);
    c.ret(rec.done(found == state.windows.end()
                       ? kErrAbsent
                       : newHandle(world, Kind::Window, player->name, found->second.container)));
}

void ofEntity(void* u, Call& c) {
    World& world = worldOf(u);
    seed(stateOf(world));
    Rec rec("inventory_of_entity");
    rec.i(c.i32(0));
    entitiesSim::Entity* entity = nullptr;
    const std::int32_t status = entitiesSim::resolve(entitiesSim::sim(world), c.i32(0), entity);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    c.ret(rec.done(entity->player ? ownedHandle(world, entity->playerHandle, Kind::Player) : kErrAbsent));
}

void release(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    const std::int32_t handle = c.i32(0);
    Rec rec("inventory_release");
    rec.i(handle);
    if (handle <= 0 || static_cast<std::size_t>(handle) > state.handles.size()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    Inventory& inventory = state.handles[static_cast<std::size_t>(handle - 1)];
    c.ret(rec.done(inventory.released ? kErrStale : kOk));
    inventory.released = true;
}

[[nodiscard]] std::int32_t windowOwn(const Resolved& r) { return static_cast<std::int32_t>(r.slots->size()); }

[[nodiscard]] Slot* slotAt(Resolved& r, std::int32_t slot) {
    if (slot == -1 && r.cursor != nullptr) {
        return r.cursor;
    }
    if (slot < 0 || slot >= windowOwn(r)) {
        return nullptr;
    }
    return &(*r.slots)[static_cast<std::size_t>(slot)];
}

void size(void* u, Call& c) {
    Resolved r;
    const std::int32_t status = resolve(worldOf(u), c.i32(0), r);
    c.ret(Rec("inventory_size").i(c.i32(0)).done(status != kOk ? status : windowOwn(r)));
}

void get(void* u, Call& c) {
    Resolved r;
    Rec rec("inventory_get");
    rec.i(c.i32(0)).i(c.i32(1)).cap(c.i32(3));
    std::int32_t status = resolve(worldOf(u), c.i32(0), r);
    Slot* slot = status == kOk ? slotAt(r, c.i32(1)) : nullptr;
    if (status == kOk && slot == nullptr) {
        status = kErrInvalid;
    }
    c.ret(status != kOk ? rec.done(status) : doneValue(rec, c.memory(), c.i32(2), c.i32(3), itemValue(*slot)));
}

void set(void* u, Call& c) {
    World& world = worldOf(u);
    Resolved r;
    const std::expected<Slot, std::int32_t> item = readSlotValue(c.memory(), c.i32(2), c.i32(3));
    Rec rec("inventory_set");
    rec.i(c.i32(0)).i(c.i32(1)).value(item ? itemValue(*item) : Value{});
    std::int32_t status = resolve(world, c.i32(0), r);
    Slot* slot = status == kOk ? slotAt(r, c.i32(1)) : nullptr;
    if (status == kOk && (slot == nullptr || !item)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        *slot = *item;
    }
    c.ret(rec.done(status));
    if (status == kOk) {
        emitLine("world inventory " + where(*r.inventory) + " slot " + std::to_string(c.i32(1)) + " = " +
                 canonical(itemValue(*slot)));
    }
}

[[nodiscard]] std::vector<std::int32_t> addOrder(const Resolved& r) {
    std::vector<std::int32_t> order;
    const auto total = static_cast<std::int32_t>(r.slots->size());
    if (r.inventory->kind == Kind::Player) {
        for (std::int32_t slot = kHotbarStart; slot < kStorageEnd; ++slot) {
            order.push_back(slot);
        }
        for (std::int32_t slot = 0; slot < kHotbarStart; ++slot) {
            order.push_back(slot);
        }
        return order;
    }
    for (std::int32_t slot = 0; slot < total - (r.cursor != nullptr ? kWindowPlayerSlots : 0); ++slot) {
        order.push_back(slot);
    }
    return order;
}

void add(void* u, Call& c) {
    World& world = worldOf(u);
    Resolved r;
    const std::expected<Slot, std::int32_t> item = readSlotValue(c.memory(), c.i32(1), c.i32(2));
    Rec rec("inventory_add");
    rec.i(c.i32(0)).value(item ? itemValue(*item) : Value{});
    std::int32_t status = resolve(world, c.i32(0), r);
    if (status == kOk && (!item || !item->has_value())) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    Item rest = **item;
    const std::vector<std::int32_t> order = addOrder(r);
    for (const std::int32_t index : order) {
        Slot& slot = (*r.slots)[static_cast<std::size_t>(index)];
        if (slot && slot->sameKind(rest) && slot->count < maxStack(*slot)) {
            const std::int64_t moved = std::min(rest.count, maxStack(*slot) - slot->count);
            slot->count += moved;
            rest.count -= moved;
        }
    }
    for (const std::int32_t index : order) {
        Slot& slot = (*r.slots)[static_cast<std::size_t>(index)];
        if (rest.count > 0 && !slot) {
            slot = rest;
            slot->count = std::min(rest.count, maxStack(rest));
            rest.count -= slot->count;
        }
    }
    c.ret(rec.done(static_cast<std::int32_t>(rest.count)));
    emitLine("world inventory " + where(*r.inventory) + " added " + canonical(itemValue(*item)) +
             " leftover=" + std::to_string(rest.count));
}

void remove(void* u, Call& c) {
    World& world = worldOf(u);
    Resolved r;
    const auto id = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("inventory_remove");
    rec.i(c.i32(0)).str(id).i(c.i32(3));
    std::int32_t status = resolve(world, c.i32(0), r);
    if (status == kOk && (!id || id->empty() || c.i32(3) < 1)) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const std::string wanted = qualified(*id);
    std::int64_t left = c.i32(3);
    for (Slot& slot : *r.slots) {
        if (left > 0 && slot && slot->id == wanted) {
            const std::int64_t taken = std::min(left, slot->count);
            slot->count -= taken;
            left -= taken;
            if (slot->count == 0) {
                slot.reset();
            }
        }
    }
    c.ret(rec.done(static_cast<std::int32_t>(c.i32(3) - left)));
}

void count(void* u, Call& c) {
    Resolved r;
    const auto id = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("inventory_count");
    rec.i(c.i32(0)).str(id);
    std::int32_t status = resolve(worldOf(u), c.i32(0), r);
    if (status == kOk && (!id || id->empty())) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    std::int64_t total = 0;
    for (const Slot& slot : *r.slots) {
        total += slot && slot->id == qualified(*id) ? slot->count : 0;
    }
    c.ret(rec.done(static_cast<std::int32_t>(total)));
}

void clear(void* u, Call& c) {
    Resolved r;
    const std::int32_t status = resolve(worldOf(u), c.i32(0), r);
    Rec rec("inventory_clear");
    rec.i(c.i32(0));
    if (status == kOk) {
        const std::size_t own = r.cursor != nullptr ? r.slots->size() - kWindowPlayerSlots : r.slots->size();
        std::fill(r.slots->begin(), r.slots->begin() + static_cast<std::ptrdiff_t>(own), Slot{});
    }
    c.ret(rec.done(status));
    if (status == kOk) {
        emitLine("world inventory " + where(*r.inventory) + " cleared");
    }
}

void contents(void* u, Call& c) {
    Resolved r;
    Rec rec("inventory_contents");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = resolve(worldOf(u), c.i32(0), r);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    Value::List list;
    for (std::size_t index = 0; index < r.slots->size(); ++index) {
        if ((*r.slots)[index]) {
            list.push_back(Value{
                Value::Map{{"slot", Value{static_cast<std::int64_t>(index)}}, {"item", itemValue((*r.slots)[index])}}});
        }
    }
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2), Value{std::move(list)}));
}

void heldSlot(void* u, Call& c) {
    World& world = worldOf(u);
    Resolved r;
    const std::int32_t status = resolve(world, c.i32(0), r);
    Rec rec("inventory_held_slot");
    rec.i(c.i32(0));
    if (status == kOk && r.inventory->kind != Kind::Player) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    c.ret(rec.done(status != kOk ? status : stateOf(world).held[r.inventory->owner]));
}

void setHeldSlot(void* u, Call& c) {
    World& world = worldOf(u);
    Resolved r;
    std::int32_t status = resolve(world, c.i32(0), r);
    Rec rec("inventory_set_held_slot");
    rec.i(c.i32(0)).i(c.i32(1));
    if (status == kOk && r.inventory->kind != Kind::Player) {
        status = kErrAbsent;
    } else if (status == kOk && (c.i32(1) < 0 || c.i32(1) > 8)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        stateOf(world).held[r.inventory->owner] = c.i32(1);
    }
    c.ret(rec.done(status));
}

void cooldown(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const auto id = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("item_cooldown");
    rec.i(c.i32(0)).str(id);
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!id || id->empty())) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const std::int64_t due = stateOf(world).cooldowns[player->name][qualified(*id)];
    c.ret(rec.done(static_cast<std::int32_t>(std::max<std::int64_t>(0, due - world.now))));
}

void setCooldown(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const auto id = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("item_set_cooldown");
    rec.i(c.i32(0)).str(id).i(c.i32(3));
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!id || id->empty())) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        stateOf(world).cooldowns[player->name][qualified(*id)] = c.i32(3) > 0 ? world.now + c.i32(3) : 0;
    }
    c.ret(rec.done(status));
}

[[nodiscard]] Value strings(const std::vector<std::string>& texts) {
    Value::List list;
    for (const std::string& text : texts) {
        list.emplace_back(text);
    }
    return Value{std::move(list)};
}

void registryGet(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto kind = memory.string(c.i32(0), c.i32(1));
    const auto key = memory.string(c.i32(2), c.i32(3));
    Rec rec("registry_get");
    rec.str(kind).str(key).cap(c.i32(5));
    if (!kind || !key) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    Value answer;
    if (*kind == "item") {
        const std::string id = qualified(*key);
        if (!known(id)) {
            c.ret(rec.done(kErrAbsent));
            return;
        }
        answer = Value{Value::Map{{"max_stack_size", Value{maxStack(Item{.id = id})}},
                                  {"max_damage", Value{id == "minecraft:diamond_sword" ? 1561 : 0}}}};
    } else if (*kind == "items") {
        answer = strings({std::begin(kKnownItems), std::end(kKnownItems)});
    } else if (*kind == "creative_tab") {
        answer = *key == "food_and_drinks" ? strings({"minecraft:apple", "minecraft:bread"}) : strings({});
    } else if (*kind == "enchantment") {
        if (qualified(*key) != "minecraft:sharpness") {
            c.ret(rec.done(kErrAbsent));
            return;
        }
        answer = Value{Value::Map{{"max_level", Value{5}}, {"weight", Value{10}}}};
    } else if (*kind == "enchantments") {
        answer = strings({"minecraft:sharpness", "minecraft:unbreaking"});
    } else if (*kind == "tags") {
        answer = Value{Value::Map{{"minecraft:swords", strings({"minecraft:diamond_sword"})}}};
    } else if (*kind == "mob_effects") {
        answer = Value{Value::List{Value{
            Value::Map{{"id", Value{"minecraft:speed"}}, {"protocol_id", Value{0}}, {"colour", Value{3402751}}}}}};
    } else if (*kind == "potions") {
        answer = Value{Value::List{Value{Value::Map{{"id", Value{"minecraft:swiftness"}}, {"effects", strings({})}}}}};
    } else {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    c.ret(doneValue(rec, memory, c.i32(4), c.i32(5), answer));
}

[[nodiscard]] double number(const Value* value, double fallback) {
    if (value == nullptr) {
        return fallback;
    }
    if (const double* real = value->asFloat()) {
        return *real;
    }
    if (const std::int64_t* whole = value->asInt()) {
        return static_cast<double>(*whole);
    }
    return fallback;
}

[[nodiscard]] double levelValue(const Value& spec, int level, int depth) {
    if (depth > 16 || (!spec.asMap() && spec.isNull())) {
        return 0.0;
    }
    if (!spec.asMap()) {
        return number(&spec, 0.0);
    }
    std::string type = spec.find("type") && spec.find("type")->asString() ? *spec.find("type")->asString() : "constant";
    if (type.starts_with("minecraft:")) {
        type = type.substr(10);
    }
    const auto child = [&](std::string_view key) {
        const Value* found = spec.find(key);
        return found == nullptr ? 0.0 : levelValue(*found, level, depth + 1);
    };
    double result = number(spec.find("value"), 0.0);
    if (type == "linear") {
        result = number(spec.find("base"), 0.0) + (number(spec.find("per_level_above_first"), 0.0) * (level - 1));
    } else if (type == "levels_squared") {
        result = static_cast<double>(level) * level + number(spec.find("added"), 0.0);
    } else if (type == "clamped") {
        result = std::clamp(child("value"), number(spec.find("min"), 0.0), number(spec.find("max"), 0.0));
    } else if (type == "fraction") {
        const double denominator = child("denominator");
        result = denominator == 0.0 ? 0.0 : child("numerator") / denominator;
    } else if (type == "exponent") {
        result = std::pow(child("base"), child("power"));
    } else if (type == "lookup") {
        const Value* values = spec.find("values");
        const Value::List* list = values == nullptr ? nullptr : values->asList();
        result = list != nullptr && level >= 1 && static_cast<std::size_t>(level) <= list->size()
                     ? number(&(*list)[static_cast<std::size_t>(level - 1)], 0.0)
                     : child("fallback");
    }
    return result;
}

void enchantLevelValue(void*, Call& c) {
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(0), c.i32(1));
    Rec rec("enchantment_level_value");
    rec.value(spec).i(c.i32(2));
    c.ret(rec.done(spec ? static_cast<double>(static_cast<float>(levelValue(*spec, c.i32(2), 0))) : 0.0));
}
} // namespace

bool Item::sameKind(const Item& other) const {
    return id == other.id && name == other.name && lore == other.lore && damage == other.damage &&
           enchantments == other.enchantments && components == other.components;
}

std::int64_t maxStack(const Item& item) {
    if (item.id == "minecraft:diamond_sword") {
        return 1;
    }
    return item.id == "minecraft:ender_pearl" ? 16 : 64;
}

std::optional<Item> parseItem(const Value& value) {
    if (value.asMap() == nullptr) {
        return std::nullopt;
    }
    const Value* id = value.find("id");
    if (id == nullptr || id->asString() == nullptr || id->asString()->empty()) {
        return std::nullopt;
    }
    Item item;
    item.id = qualified(*id->asString());
    if (!known(item.id)) {
        return std::nullopt;
    }
    if (const Value* count = value.find("count")) {
        if (count->asInt() == nullptr) {
            return std::nullopt;
        }
        item.count = std::max<std::int64_t>(1, *count->asInt());
    }
    if (const Value* name = value.find("name"); name != nullptr && !name->isNull()) {
        if (name->asString() == nullptr) {
            return std::nullopt;
        }
        item.name = *name->asString();
    }
    if (const Value* lore = value.find("lore"); lore != nullptr && !lore->isNull()) {
        if (lore->asList() == nullptr) {
            return std::nullopt;
        }
        for (const Value& line : *lore->asList()) {
            if (line.asString() == nullptr) {
                return std::nullopt;
            }
            item.lore.push_back(*line.asString());
        }
    }
    if (const Value* damage = value.find("damage"); damage != nullptr && !damage->isNull()) {
        if (damage->asInt() == nullptr) {
            return std::nullopt;
        }
        item.damage = std::max<std::int64_t>(0, *damage->asInt());
    }
    if (const Value* enchantments = value.find("enchantments"); enchantments != nullptr && !enchantments->isNull()) {
        if (enchantments->asMap() == nullptr) {
            return std::nullopt;
        }
        for (const MapEntry& entry : *enchantments->asMap()) {
            if (entry.value.asInt() == nullptr) {
                return std::nullopt;
            }
            item.enchantments[qualified(entry.key)] = *entry.value.asInt();
        }
    }
    if (const Value* components = value.find("components"); components != nullptr && !components->isNull()) {
        if (components->asMap() == nullptr) {
            return std::nullopt;
        }
        for (const MapEntry& entry : *components->asMap()) {
            if (entry.value.asString() == nullptr || entry.value.asString()->empty()) {
                return std::nullopt;
            }
            item.components[qualified(entry.key)] = *entry.value.asString();
        }
    }
    return item;
}

Value itemValue(const Slot& item) {
    if (!item) {
        return Value{};
    }
    Value::Map map{{"id", Value{item->id}}, {"count", Value{item->count}}};
    if (item->name) {
        map.push_back({"name", Value{*item->name}});
    }
    if (!item->lore.empty()) {
        map.push_back({"lore", strings(item->lore)});
    }
    if (item->damage > 0) {
        map.push_back({"damage", Value{item->damage}});
    }
    if (!item->enchantments.empty()) {
        Value::Map levels;
        for (const auto& [id, level] : item->enchantments) {
            levels.push_back({id, Value{level}});
        }
        map.push_back({"enchantments", Value{std::move(levels)}});
    }
    if (!item->components.empty()) {
        Value::Map components;
        for (const auto& [id, snbt] : item->components) {
            components.push_back({id, Value{snbt}});
        }
        map.push_back({"components", Value{std::move(components)}});
    }
    return Value{std::move(map)};
}

std::expected<Slot, std::int32_t> readSlotValue(const GuestMemory& memory, std::int32_t ptr, std::int32_t len) {
    const std::expected<Value, std::int32_t> value = readValue(memory, ptr, len);
    if (!value) {
        return std::unexpected(value.error());
    }
    if (value->isNull()) {
        return Slot{};
    }
    std::optional<Item> item = parseItem(*value);
    if (!item) {
        return std::unexpected(kErrInvalid);
    }
    return Slot{std::move(*item)};
}

State& stateOf(World& world) { return world.areaState<State>("items"); }

std::int32_t doneValue(Rec& rec, const GuestMemory& memory, std::int32_t ptr, std::int32_t cap, const Value& value) {
    const std::int32_t result = putValue(memory, ptr, cap, value);
    return result >= 0 && result <= cap ? rec.done(result, canonical(value)) : rec.done(result);
}

void seedState(World& world) { seed(stateOf(world)); }

} // namespace basalt::wire::wasm::trace::items

namespace basalt::wire::wasm::trace {
void addItemsImports(World& world, std::vector<HostImport>& out) {
    using namespace items;
    World* user = &world;
    out.insert(out.end(), {
                              {"inventory_of_player", ofPlayer, user},
                              {"inventory_of_block", ofBlock, user},
                              {"inventory_of_window", ofWindow, user},
                              {"inventory_of_ender", ofEnder, user},
                              {"inventory_of_entity", ofEntity, user},
                              {"inventory_release", release, user},
                              {"inventory_size", size, user},
                              {"inventory_get", get, user},
                              {"inventory_set", set, user},
                              {"inventory_add", add, user},
                              {"inventory_remove", remove, user},
                              {"inventory_count", count, user},
                              {"inventory_clear", clear, user},
                              {"inventory_contents", contents, user},
                              {"inventory_held_slot", heldSlot, user},
                              {"inventory_set_held_slot", setHeldSlot, user},
                              {"item_cooldown", cooldown, user},
                              {"item_set_cooldown", setCooldown, user},
                              {"registry_get", registryGet, user},
                              {"enchantment_level_value", enchantLevelValue, user},
                          });
}
} // namespace basalt::wire::wasm::trace
