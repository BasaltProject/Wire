#include "HostItems.hpp"
#include "Scenario.hpp"

#include <algorithm>

namespace basalt::wire::wasm::trace {
namespace {
using namespace items;

constexpr std::int32_t kPlayerPart = 36;
constexpr const char* kMenuClickEvent = "items.menu_click";

struct WindowKind {
    const char* name;
    std::int32_t size;
};

constexpr WindowKind kKinds[] = {{"chest", 27},    {"barrel", 27},   {"shulker_box", 27}, {"hopper", 5},
                                 {"dispenser", 9}, {"dropper", 9},   {"furnace", 3},      {"anvil", 3},
                                 {"smithing", 4},  {"crafting", 10}, {"merchant", 3}};

[[nodiscard]] const WindowKind* kindNamed(std::string_view name) {
    for (const WindowKind& kind : kKinds) {
        if (name == kind.name) {
            return &kind;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string qualifiedId(std::string_view id) {
    return id.find(':') == std::string_view::npos ? "minecraft:" + std::string{id} : std::string{id};
}

[[nodiscard]] const Value* field(const Value& map, std::string_view key) {
    const Value* found = map.find(key);
    return found == nullptr || found->isNull() ? nullptr : found;
}

[[nodiscard]] std::string textOr(const Value& map, std::string_view key, std::string fallback) {
    const Value* found = field(map, key);
    return found != nullptr && found->asString() != nullptr ? *found->asString() : std::move(fallback);
}

void recipeAdd(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    seedState(world);
    const GuestMemory memory = c.memory();
    const auto id = memory.string(c.i32(0), c.i32(1));
    const auto json = memory.string(c.i32(2), c.i32(3));
    Rec rec("recipe_add");
    rec.str(id).str(json);
    if (!id || !json || id->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::size_t key = json->find("\"type\"");
    const std::size_t open = key == std::string_view::npos ? key : json->find('"', json->find(':', key) + 1);
    const std::size_t close = open == std::string_view::npos ? open : json->find('"', open + 1);
    const std::string name = qualifiedId(*id);
    if (json->empty() || json->front() != '{' || close == std::string_view::npos ||
        std::find(state.recipes.begin(), state.recipes.end(), name) != state.recipes.end()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    state.recipes.push_back(name);
    state.recipeTypes[name] = std::string{json->substr(open + 1, close - open - 1)};
    c.ret(rec.done(kOk));
    emitLine("world recipe added " + name);
}

void recipeRemove(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    seedState(world);
    const auto id = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("recipe_remove");
    rec.str(id);
    if (!id || id->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::string name = qualifiedId(*id);
    const auto found = std::find(state.recipes.begin(), state.recipes.end(), name);
    if (found == state.recipes.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    state.recipes.erase(found);
    state.recipeTypes.erase(name);
    c.ret(rec.done(kOk));
    emitLine("world recipe removed " + name);
}

void recipeHas(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    seedState(world);
    const auto id = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("recipe_has");
    rec.str(id);
    if (!id || id->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    c.ret(rec.done(std::find(state.recipes.begin(), state.recipes.end(), qualifiedId(*id)) != state.recipes.end() ? 1
                                                                                                                  : 0));
}

[[nodiscard]] Value idList(const std::vector<std::string>& ids) {
    Value::List list;
    for (const std::string& id : ids) {
        list.emplace_back(id);
    }
    return Value{std::move(list)};
}

void recipeIds(void* u, Call& c) {
    World& world = worldOf(u);
    seedState(world);
    Rec rec("recipe_ids");
    rec.cap(c.i32(1));
    c.ret(doneValue(rec, c.memory(), c.i32(0), c.i32(1), idList(stateOf(world).recipes)));
}

void recipeClearCall(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    seedState(world);
    state.recipes.clear();
    state.recipeTypes.clear();
    c.ret(Rec("recipe_clear").done(kOk));
    emitLine("world recipes cleared");
}

void recipeReset(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    seedState(world);
    state.recipes = {"minecraft:oak_planks", "minecraft:stick"};
    state.recipeTypes = {{"minecraft:oak_planks", "minecraft:crafting_shapeless"},
                         {"minecraft:stick", "minecraft:crafting_shaped"}};
    c.ret(Rec("recipe_reset").done(kOk));
    emitLine("world recipes reset");
}

void recipeResetBrewing(void*, Call& c) { c.ret(Rec("recipe_reset_brewing").done(kOk)); }

void recipeGet(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    seedState(world);
    const auto id = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("recipe_get");
    rec.str(id).cap(c.i32(3));
    if (!id || id->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::string name = qualifiedId(*id);
    const auto found = state.recipeTypes.find(name);
    if (found == state.recipeTypes.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    c.ret(doneValue(rec, c.memory(), c.i32(2), c.i32(3),
                    Value{Value::Map{{"id", Value{name}}, {"type", Value{found->second}}}}));
}

[[nodiscard]] std::int32_t station(const std::string& kind, const Value& input, Value& answer) {
    if (input.asMap() == nullptr) {
        return kErrInvalid;
    }
    if (kind == "crafting") {
        const Value* width = input.find("width");
        const Value* height = input.find("height");
        const Value* list = input.find("items");
        if (width == nullptr || height == nullptr || list == nullptr || width->asInt() == nullptr ||
            height->asInt() == nullptr || list->asList() == nullptr) {
            return kErrInvalid;
        }
        bool logs = false;
        for (const Value& entry : *list->asList()) {
            if (!entry.isNull() && parseItem(entry).has_value() == false) {
                return kErrInvalid;
            }
            logs = logs || (!entry.isNull() && parseItem(entry)->id == "minecraft:oak_log");
        }
        if (!logs) {
            return kErrAbsent;
        }
        Value::List remaining(list->asList()->size());
        answer = Value{Value::Map{{"recipe", Value{"minecraft:oak_planks"}},
                                  {"result", itemValue(Item{.id = "minecraft:oak_planks", .count = 4})},
                                  {"remaining", Value{std::move(remaining)}}}};
        return kOk;
    }
    const auto stack = [&input](std::string_view key) -> std::expected<Slot, std::int32_t> {
        const Value* found = input.find(key);
        if (found == nullptr || found->isNull()) {
            return Slot{};
        }
        std::optional<Item> item = parseItem(*found);
        if (!item) {
            return std::unexpected(kErrInvalid);
        }
        return Slot{std::move(*item)};
    };
    if (kind == "anvil") {
        const auto left = stack("left");
        const auto right = stack("right");
        if (!left || !right) {
            return kErrInvalid;
        }
        const std::string name = textOr(input, "name", "");
        Slot result = *left;
        if (result && !name.empty()) {
            result->name = name;
        }
        answer = Value{Value::Map{{"result", itemValue(result)},
                                  {"cost", Value{result ? 1 + (name.empty() ? 0 : 1) : 0}},
                                  {"materials", Value{*right ? 1 : 0}}}};
        return kOk;
    }
    if (kind == "smithing") {
        const auto base = stack("base");
        const auto addition = stack("addition");
        if (!stack("template") || !base || !addition) {
            return kErrInvalid;
        }
        if (!*base || !*addition) {
            return kErrAbsent;
        }
        Slot result = *base;
        result->count = 1;
        answer = Value{Value::Map{{"recipe", Value{"minecraft:smithing_demo"}}, {"result", itemValue(result)}}};
        return kOk;
    }
    return kErrInvalid;
}

void recipeStation(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto kind = memory.string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> input = readValue(memory, c.i32(2), c.i32(3));
    Rec rec("recipe_station");
    rec.str(kind).value(input).cap(c.i32(5));
    Value answer;
    const std::int32_t status = !kind || !input ? kErrInvalid : station(std::string{*kind}, *input, answer);
    c.ret(status != kOk ? rec.done(status) : doneValue(rec, memory, c.i32(4), c.i32(5), answer));
}

void bookGet(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    Rec rec("recipe_book_get");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    c.ret(status != kOk ? rec.done(status)
                        : doneValue(rec, c.memory(), c.i32(1), c.i32(2), idList(stateOf(world).book[player->name])));
}

void bookSet(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    seedState(world);
    Player* player = nullptr;
    const std::expected<Value, std::int32_t> ids = readValue(c.memory(), c.i32(1), c.i32(2));
    Rec rec("recipe_book_set");
    rec.i(c.i32(0)).value(ids).i(c.i32(3));
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!ids || ids->asList() == nullptr)) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    std::vector<std::string>& known = state.book[player->name];
    std::int32_t changed = 0;
    for (const Value& entry : *ids->asList()) {
        if (entry.asString() == nullptr) {
            c.ret(rec.done(kErrInvalid));
            return;
        }
        const std::string name = qualifiedId(*entry.asString());
        const auto at = std::find(known.begin(), known.end(), name);
        const bool exists = std::find(state.recipes.begin(), state.recipes.end(), name) != state.recipes.end();
        if (c.i32(3) != 0 && at == known.end() && exists) {
            known.push_back(name);
            ++changed;
        } else if (c.i32(3) == 0 && at != known.end()) {
            known.erase(at);
            ++changed;
        }
    }
    c.ret(rec.done(changed));
}

void bookHas(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const auto id = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("recipe_book_has");
    rec.i(c.i32(0)).str(id);
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!id || id->empty())) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const std::vector<std::string>& known = stateOf(world).book[player->name];
    c.ret(rec.done(std::find(known.begin(), known.end(), qualifiedId(*id)) != known.end() ? 1 : 0));
}

void bookResend(void* u, Call& c) {
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(worldOf(u), c.i32(0), player);
    c.ret(Rec("recipe_book_resend").i(c.i32(0)).done(status));
    if (status == kOk) {
        emitLine("world recipe-book resent " + player->name);
    }
}

void merchantOpen(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(1), c.i32(2));
    Rec rec("merchant_open");
    rec.i(c.i32(0)).value(spec);
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    const Value* offers = status == kOk && spec && spec->asMap() != nullptr ? spec->find("offers") : nullptr;
    if (status == kOk && (offers == nullptr || offers->asList() == nullptr)) {
        status = kErrInvalid;
    }
    for (std::size_t index = 0; status == kOk && index < offers->asList()->size(); ++index) {
        const Value& offer = (*offers->asList())[index];
        const Value* buy = offer.find("buy");
        const Value* sell = offer.find("sell");
        if (buy == nullptr || sell == nullptr || !parseItem(*buy) || !parseItem(*sell)) {
            status = kErrInvalid;
        }
    }
    c.ret(rec.done(status));
    if (status == kOk) {
        emitLine("world merchant " + player->name + " " + quote(textOr(*spec, "title", "")) +
                 " offers=" + std::to_string(offers->asList()->size()));
    }
}

[[nodiscard]] Window buildWindow(World& world, const std::string& kind, const std::string& title,
                                 std::int32_t slotCount, bool plugin) {
    Window window;
    window.container = stateOf(world).nextContainer++;
    window.kind = kind;
    window.title = title;
    window.plugin = plugin;
    window.slots.resize(static_cast<std::size_t>(slotCount + kPlayerPart));
    return window;
}

void windowOpen(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(1), c.i32(2));
    Rec rec("window_open");
    rec.i(c.i32(0)).value(spec);
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!spec || spec->asMap() == nullptr)) {
        status = kErrInvalid;
    }
    const std::string kind = status == kOk ? textOr(*spec, "kind", "chest") : "";
    const WindowKind* known = status == kOk ? kindNamed(kind) : nullptr;
    if (status == kOk && known == nullptr) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    std::int32_t size = known->size;
    if (kind == "chest") {
        const Value* wanted = field(*spec, "size");
        const std::int64_t rows = wanted != nullptr && wanted->asInt() != nullptr
                                      ? std::clamp<std::int64_t>((*wanted->asInt() + 8) / 9, 1, 6)
                                      : 3;
        size = static_cast<std::int32_t>(rows * 9);
    }
    Window window = buildWindow(world, kind, textOr(*spec, "title", ""), size, true);
    if (const Value* items = field(*spec, "items"); items != nullptr && items->asList() != nullptr) {
        for (std::size_t slot = 0; slot < items->asList()->size() && slot < static_cast<std::size_t>(size); ++slot) {
            const Value& entry = (*items->asList())[slot];
            if (!entry.isNull()) {
                window.slots[slot] = parseItem(entry);
            }
        }
    }
    const std::int32_t container = window.container;
    openWindowFor(world, *player, std::move(window));
    c.ret(rec.done(container));
}

void windowInfo(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    Rec rec("window_info");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const auto found = stateOf(world).windows.find(player->name);
    if (found == stateOf(world).windows.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const Window& window = found->second;
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2),
                    Value{Value::Map{{"container_id", Value{window.container}},
                                     {"kind", Value{window.kind}},
                                     {"slots", Value{static_cast<std::int64_t>(window.slots.size())}},
                                     {"plugin", Value{window.plugin}},
                                     {"world", Value{"world"}}}}));
}

void windowClose(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    Rec rec("window_close");
    rec.i(c.i32(0));
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk) {
        status = stateOf(world).windows.erase(player->name) == 0 ? kErrAbsent : kOk;
    }
    c.ret(rec.done(status));
    if (status == kOk) {
        emitLine("world window closed " + player->name);
    }
}

void windowUpdate(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(1), c.i32(2));
    Rec rec("window_update");
    rec.i(c.i32(0)).value(spec);
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!spec || spec->asMap() == nullptr)) {
        status = kErrInvalid;
    }
    const auto found = status == kOk ? stateOf(world).windows.find(player->name) : stateOf(world).windows.end();
    if (status == kOk && found == stateOf(world).windows.end()) {
        status = kErrAbsent;
    }
    if (status == kOk) {
        if (const Value* title = field(*spec, "title"); title != nullptr && title->asString() != nullptr) {
            found->second.title = *title->asString();
            emitLine("world window title " + player->name + " " + quote(found->second.title));
        }
        if (const Value* props = field(*spec, "properties"); props != nullptr && props->asMap() != nullptr) {
            for (const MapEntry& entry : *props->asMap()) {
                if (entry.value.asInt() == nullptr) {
                    status = kErrInvalid;
                    break;
                }
                found->second.properties[entry.key] = *entry.value.asInt();
                emitLine("world window property " + player->name + " " + entry.key + "=" +
                         std::to_string(*entry.value.asInt()));
            }
        }
    }
    c.ret(rec.done(status));
}

[[nodiscard]] MenuDef* menuOf(State& state, std::int32_t handle) {
    return handle >= 1 && static_cast<std::size_t>(handle) <= state.menus.size()
               ? &state.menus[static_cast<std::size_t>(handle - 1)]
               : nullptr;
}

[[nodiscard]] std::int32_t menuSize(const MenuDef& menu) {
    return menu.kind == "chest" ? static_cast<std::int32_t>(menu.rows * 9) : kindNamed(menu.kind)->size;
}

void menuCreate(void* u, Call& c) {
    World& world = worldOf(u);
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(0), c.i32(1));
    Rec rec("container_menu_create");
    rec.value(spec);
    if (!spec || spec->asMap() == nullptr) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    MenuDef menu;
    menu.title = textOr(*spec, "title", "");
    menu.kind = textOr(*spec, "kind", "chest");
    const Value* rows = field(*spec, "rows");
    menu.rows = rows != nullptr && rows->asInt() != nullptr ? *rows->asInt() : 3;
    if ((rows != nullptr && rows->asInt() == nullptr) || menu.rows < 1 || menu.rows > 6 ||
        kindNamed(menu.kind) == nullptr) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    stateOf(world).menus.push_back(std::move(menu));
    c.ret(rec.done(static_cast<std::int32_t>(stateOf(world).menus.size())));
}

void redraw(World& world, std::int32_t handle) {
    for (auto& [name, window] : stateOf(world).windows) {
        if (window.menu != handle) {
            continue;
        }
        const MenuDef& menu = stateOf(world).menus[static_cast<std::size_t>(handle - 1)];
        std::fill(window.slots.begin(), window.slots.end(), Slot{});
        for (const auto& [slot, entry] : menu.slots) {
            window.slots[static_cast<std::size_t>(slot)] = entry.item;
        }
        emitLine("world menu " + std::to_string(handle) + " redrawn for " + name);
    }
}

void menuSet(void* u, Call& c) {
    World& world = worldOf(u);
    MenuDef* menu = menuOf(stateOf(world), c.i32(0));
    const std::expected<Slot, std::int32_t> item = readSlotValue(c.memory(), c.i32(2), c.i32(3));
    Rec rec("container_menu_set");
    rec.i(c.i32(0)).i(c.i32(1)).value(item ? itemValue(*item) : Value{}).i(c.i32(4));
    if (menu == nullptr || !item || c.i32(1) < 0 || c.i32(1) >= menuSize(*menu)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (item->has_value()) {
        menu->slots[c.i32(1)] = MenuSlotDef{*item, c.i32(4)};
    } else {
        menu->slots.erase(c.i32(1));
    }
    c.ret(rec.done(kOk));
    redraw(world, c.i32(0));
}

void menuClear(void* u, Call& c) {
    World& world = worldOf(u);
    MenuDef* menu = menuOf(stateOf(world), c.i32(0));
    Rec rec("container_menu_clear");
    rec.i(c.i32(0));
    if (menu == nullptr) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    menu->slots.clear();
    c.ret(rec.done(kOk));
    redraw(world, c.i32(0));
}

void menuOpen(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    MenuDef* menu = menuOf(stateOf(world), c.i32(0));
    Rec rec("container_menu_open");
    rec.i(c.i32(0)).i(c.i32(1));
    const std::int32_t status = checkPlayer(world, c.i32(1), player);
    if (menu == nullptr || status != kOk) {
        c.ret(rec.done(menu == nullptr ? kErrInvalid : status));
        return;
    }
    Window window = buildWindow(world, menu->kind, menu->title, menuSize(*menu), true);
    window.menu = c.i32(0);
    for (const auto& [slot, entry] : menu->slots) {
        window.slots[static_cast<std::size_t>(slot)] = entry.item;
    }
    openWindowFor(world, *player, std::move(window));
    c.ret(rec.done(kOk));
}

void menuClose(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    MenuDef* menu = menuOf(stateOf(world), c.i32(0));
    Rec rec("container_menu_close");
    rec.i(c.i32(0)).i(c.i32(1));
    std::int32_t status = checkPlayer(world, c.i32(1), player);
    if (menu == nullptr) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        const auto found = stateOf(world).windows.find(player->name);
        status = found != stateOf(world).windows.end() && found->second.menu == c.i32(0) ? kOk : kErrAbsent;
        if (status == kOk) {
            stateOf(world).windows.erase(found);
            emitLine("world window closed " + player->name);
        }
    }
    c.ret(rec.done(status));
}

[[nodiscard]] std::int32_t clickContext(World& world, std::int32_t id, Context*& out) {
    const std::int32_t status = world.checkContext(id, ContextKind::Event, out);
    return status == kOk && out->eventName != kMenuClickEvent ? kErrInvalid : status;
}

void clickInfo(void* u, Call& c) {
    World& world = worldOf(u);
    Context* context = nullptr;
    Rec rec("container_menu_click_info");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = clickContext(world, c.i32(0), context);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2),
                    Value{Value::Map{{"player", Value{context->player}},
                                     {"slot", Value{std::get<std::int64_t>(context->fields["slot"])}},
                                     {"button", Value{std::get<std::int64_t>(context->fields["button"])}},
                                     {"mode", Value{"pickup"}}}}));
}

void clickResult(void* u, Call& c) {
    World& world = worldOf(u);
    Context* context = nullptr;
    const std::expected<Slot, std::int32_t> item = readSlotValue(c.memory(), c.i32(1), c.i32(2));
    Rec rec("container_menu_click_result");
    rec.i(c.i32(0)).value(item ? itemValue(*item) : Value{});
    std::int32_t status = clickContext(world, c.i32(0), context);
    if (status == kOk && !item) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        context->value = itemValue(*item);
    }
    c.ret(rec.done(status));
}

void clickCancel(void* u, Call& c) {
    World& world = worldOf(u);
    Context* context = nullptr;
    Rec rec("container_menu_click_cancel");
    rec.i(c.i32(0)).i(c.i32(1));
    std::int32_t status = clickContext(world, c.i32(0), context);
    if (status == kOk && c.i32(1) == 0) {
        status = kErrDenied;
    }
    c.ret(rec.done(status));
}

[[nodiscard]] bool toInt(std::string_view text, std::int64_t& out) {
    try {
        std::size_t used = 0;
        out = std::stoll(std::string{text}, &used);
        return used == text.size() && !text.empty();
    } catch (...) {
        return false;
    }
}

[[nodiscard]] std::string_view word(std::string_view& rest) {
    while (!rest.empty() && rest.front() == ' ') {
        rest.remove_prefix(1);
    }
    const std::size_t end = rest.find(' ');
    const std::string_view taken = rest.substr(0, end);
    rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end);
    return taken;
}

bool clickVerb(World& world, std::string_view rest) {
    std::int64_t menu = 0;
    std::int64_t slot = 0;
    std::int64_t button = 0;
    const std::string_view menuText = word(rest);
    const std::string_view slotText = word(rest);
    const std::string_view playerName = word(rest);
    const std::string_view buttonText = word(rest);
    if (!toInt(menuText, menu) || !toInt(slotText, slot) || (!buttonText.empty() && !toInt(buttonText, button))) {
        return false;
    }
    State& state = stateOf(world);
    Player* player = world.playerByName(playerName);
    MenuDef* def = menuOf(state, static_cast<std::int32_t>(menu));
    if (player == nullptr || !player->online || def == nullptr) {
        emitLine("world click-rejected items menu=" + std::to_string(menu));
        return true;
    }
    const auto open = state.windows.find(player->name);
    const auto entry = def->slots.find(static_cast<std::int32_t>(slot));
    if (open == state.windows.end() || open->second.menu != menu || def->kind != "chest" || entry == def->slots.end() ||
        entry->second.token == 0) {
        emitLine("world click-ignored items menu=" + std::to_string(menu) + " slot=" + std::to_string(slot));
        return true;
    }
    Context context;
    context.kind = ContextKind::Event;
    context.eventName = kMenuClickEvent;
    context.player = player->handle;
    context.fields["slot"] = slot;
    context.fields["button"] = button;
    static_cast<void>(world.invoke(entry->second.token, std::move(context)));
    if (world.current.value) {
        const Slot replaced = parseItem(*world.current.value);
        entry->second.item = world.current.value->isNull() ? Slot{} : replaced;
        if (entry->second.item) {
            open->second.slots[static_cast<std::size_t>(slot)] = entry->second.item;
        } else {
            open->second.slots[static_cast<std::size_t>(slot)].reset();
        }
        emitLine("world menu " + std::to_string(menu) + " slot " + std::to_string(slot) + " = " +
                 canonical(*world.current.value));
    }
    return true;
}
} // namespace

namespace items {
void openWindowFor(World& world, Player& player, Window window) {
    emitLine("world window opened " + player.name + " " + window.kind + " " + quote(window.title) +
             " slots=" + std::to_string(window.slots.size()));
    stateOf(world).windows[player.name] = std::move(window);
}
} // namespace items

namespace {
const ScenarioVerbRegistrar kItemsClickVerb{"items-click", clickVerb};
} // namespace

void addItemsExtraImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"recipe_add", recipeAdd, user},
                              {"recipe_remove", recipeRemove, user},
                              {"recipe_has", recipeHas, user},
                              {"recipe_ids", recipeIds, user},
                              {"recipe_clear", recipeClearCall, user},
                              {"recipe_reset", recipeReset, user},
                              {"recipe_reset_brewing", recipeResetBrewing, user},
                              {"recipe_get", recipeGet, user},
                              {"recipe_station", recipeStation, user},
                              {"recipe_book_get", bookGet, user},
                              {"recipe_book_set", bookSet, user},
                              {"recipe_book_has", bookHas, user},
                              {"recipe_book_resend", bookResend, user},
                              {"merchant_open", merchantOpen, user},
                              {"window_open", windowOpen, user},
                              {"window_info", windowInfo, user},
                              {"window_close", windowClose, user},
                              {"window_update", windowUpdate, user},
                              {"container_menu_create", menuCreate, user},
                              {"container_menu_set", menuSet, user},
                              {"container_menu_clear", menuClear, user},
                              {"container_menu_open", menuOpen, user},
                              {"container_menu_close", menuClose, user},
                              {"container_menu_click_info", clickInfo, user},
                              {"container_menu_click_result", clickResult, user},
                              {"container_menu_click_cancel", clickCancel, user},
                          });
}

} // namespace basalt::wire::wasm::trace
