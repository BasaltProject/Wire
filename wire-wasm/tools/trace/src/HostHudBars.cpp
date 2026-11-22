#include "HostHud.hpp"
#include "Scenario.hpp"

#include <algorithm>
#include <array>

namespace basalt::wire::wasm::trace {
namespace hud {
namespace {
constexpr std::array<std::string_view, 8> kPlayerProperties{
    "tab_header", "tab_footer", "list_name", "listed", "list_order", "score", "waypoint_style", "waypoint_color"};
constexpr std::array<std::string_view, 5> kSurfaces{"sidebar", "tab_list", "boss_bar", "chat_format", "name_tag"};

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

[[nodiscard]] Value barMap(const Bar& bar) {
    Value::Map map;
    map.push_back({"id", Value{bar.id}});
    map.push_back({"key", bar.keyed ? Value{bar.id} : Value{}});
    for (const std::string_view key :
         {"title", "color", "style", "progress", "darken_sky", "boss_music", "fog", "visible"}) {
        map.push_back({std::string{key}, bar.fields.at(std::string{key})});
    }
    return Value{std::move(map)};
}

[[nodiscard]] std::int32_t fillBar(World& world, const Value& spec, Bar& bar) {
    std::int32_t status = kOk;
    for (const std::string_view key : {"title"}) {
        std::optional<std::string> text;
        status = status != kOk ? status : takeText(world, spec, key, text);
        if (text) {
            bar.fields[std::string{key}] = Value{*text};
        }
    }
    for (const std::string_view key : {"color", "style"}) {
        std::optional<std::string> word;
        status = status != kOk ? status : takeString(spec, key, word);
        if (word) {
            bar.fields[std::string{key}] = Value{*word};
        }
    }
    std::optional<double> progress;
    status = status != kOk ? status : takeNumber(spec, "progress", progress);
    if (progress) {
        bar.fields["progress"] = Value{*progress};
    }
    for (const std::string_view key : {"darken_sky", "boss_music", "fog", "visible"}) {
        std::optional<bool> flag;
        status = status != kOk ? status : takeBool(spec, key, flag);
        if (flag) {
            bar.fields[std::string{key}] = Value{*flag};
        }
    }
    return status;
}

void bossbarCreate(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(0), c.i32(1));
    Rec rec("hud_bossbar_create");
    rec.value(spec);
    if (!spec) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    Bar bar;
    bar.fields = {{"title", Value{""}},     {"color", Value{"purple"}},   {"style", Value{"progress"}},
                  {"progress", Value{1.0}}, {"darken_sky", Value{false}}, {"boss_music", Value{false}},
                  {"fog", Value{false}},    {"visible", Value{true}}};
    std::optional<std::string> key;
    std::int32_t status = fillBar(world, *spec, bar);
    status = status != kOk ? status : takeString(*spec, "key", key);
    if (status != kOk) {
        c.ret(rec.done(status == kErrAbsent ? status : kErrInvalid));
        return;
    }
    if (key) {
        const bool taken =
            std::ranges::any_of(state.bars, [&](const Bar& held) { return held.alive && held.id == *key; });
        if (taken) {
            c.ret(rec.done(kErrInvalid));
            return;
        }
        bar.id = *key;
        bar.keyed = true;
    } else {
        bar.id = "bar-" + std::to_string(state.nextBar++);
        bar.owned = true;
    }
    state.bars.push_back(std::move(bar));
    c.ret(rec.done(static_cast<std::int32_t>(state.bars.size())));
}

void bossbarAttach(void* u, Call& c) {
    State& state = stateOf(worldOf(u));
    const auto id = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("hud_bossbar_attach");
    rec.str(id);
    if (!id || id->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    for (std::size_t at = 0; at < state.bars.size(); ++at) {
        if (state.bars[at].alive && state.bars[at].id == *id) {
            c.ret(rec.done(static_cast<std::int32_t>(at + 1)));
            return;
        }
    }
    c.ret(rec.done(kErrAbsent));
}

void bossbarUpdate(void* u, Call& c) {
    World& world = worldOf(u);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    Rec rec("hud_bossbar_update");
    rec.i(c.i32(0)).value(spec);
    Bar* bar = nullptr;
    std::int32_t status = findIn(stateOf(world).bars, c.i32(0), bar);
    if (status == kOk && !spec) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        Bar staged = *bar;
        status = fillBar(world, *spec, staged);
        status = status == kErrAbsent ? kErrInvalid : status;
        if (status == kOk) {
            *bar = std::move(staged);
        }
    }
    c.ret(rec.done(status));
}

void bossbarRemove(void* u, Call& c) {
    Bar* bar = nullptr;
    const std::int32_t status = findIn(stateOf(worldOf(u)).bars, c.i32(0), bar);
    if (status == kOk) {
        bar->alive = false;
    }
    c.ret(Rec("hud_bossbar_remove").i(c.i32(0)).done(status));
}

void bossbarPlayer(void* u, Call& c, bool add) {
    World& world = worldOf(u);
    Player* player = nullptr;
    Bar* bar = nullptr;
    std::int32_t status = findIn(stateOf(world).bars, c.i32(0), bar);
    if (status == kOk) {
        status = checkPlayer(world, c.i32(1), player);
    }
    if (status == kOk) {
        const std::string uuid = uuidOf(player->name);
        if (add && std::ranges::find(bar->players, uuid) == bar->players.end()) {
            bar->players.push_back(uuid);
        } else if (!add) {
            std::erase(bar->players, uuid);
        }
    }
    c.ret(Rec(add ? "hud_bossbar_player_add" : "hud_bossbar_player_remove").i(c.i32(0)).i(c.i32(1)).done(status));
}

void bossbarPlayerAdd(void* u, Call& c) { bossbarPlayer(u, c, true); }

void bossbarPlayerRemove(void* u, Call& c) { bossbarPlayer(u, c, false); }

void bossbarPlayers(void* u, Call& c) {
    Rec rec("hud_bossbar_players");
    rec.i(c.i32(0)).cap(c.i32(2));
    Bar* bar = nullptr;
    const std::int32_t status = findIn(stateOf(worldOf(u)).bars, c.i32(0), bar);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2), stringList(bar->players)));
}

void bossbarGet(void* u, Call& c) {
    Rec rec("hud_bossbar_get");
    rec.i(c.i32(0)).cap(c.i32(2));
    Bar* bar = nullptr;
    const std::int32_t status = findIn(stateOf(worldOf(u)).bars, c.i32(0), bar);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2), barMap(*bar)));
}

void bossbarKept(void* u, Call& c) {
    Rec rec("hud_bossbar_kept");
    rec.cap(c.i32(1));
    std::vector<std::string> ids;
    for (const Bar& bar : stateOf(worldOf(u)).bars) {
        if (bar.alive && bar.keyed) {
            ids.push_back(bar.id);
        }
    }
    c.ret(doneValue(rec, c.memory(), c.i32(0), c.i32(1), stringList(ids)));
}

[[nodiscard]] bool inGroup(std::string_view key) {
    return std::ranges::find(kPlayerProperties, key) != kPlayerProperties.end();
}

[[nodiscard]] std::map<std::string, Value>& propsOf(State& state, const Player& player) {
    std::map<std::string, Value>& props = state.playerProps[player.name];
    if (props.empty()) {
        props = {{"tab_header", Value{}},
                 {"tab_footer", Value{}},
                 {"list_name", Value{}},
                 {"listed", Value{true}},
                 {"list_order", Value{0}},
                 {"score", Value{0}},
                 {"waypoint_style", Value{"minecraft:default"}},
                 {"waypoint_color", Value{-1}}};
    }
    return props;
}

void playerSet(void* u, Call& c) {
    World& world = worldOf(u);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    Rec rec("hud_player_set");
    rec.i(c.i32(0)).value(spec);
    Player* player = nullptr;
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && !spec) {
        status = kErrInvalid;
    }
    std::vector<std::pair<std::string, Value>> changes;
    if (status == kOk) {
        for (const MapEntry& entry : *spec->asMap()) {
            const bool text = entry.key == "tab_header" || entry.key == "tab_footer" || entry.key == "list_name";
            const bool flag = entry.key == "listed";
            const bool whole = entry.key == "list_order" || entry.key == "score" || entry.key == "waypoint_color";
            const bool word = entry.key == "waypoint_style";
            const bool typed = (text && (entry.value.isNull() || entry.value.asString() != nullptr)) ||
                               (flag && entry.value.asBool() != nullptr) || (whole && entry.value.asInt() != nullptr) ||
                               (word && entry.value.asString() != nullptr);
            if (!inGroup(entry.key) || !typed) {
                status = kErrInvalid;
                break;
            }
            Value stored = entry.value;
            if (text && !entry.value.isNull()) {
                std::string rendered;
                status = renderPlain(world, *entry.value.asString(), rendered);
                stored = Value{rendered};
            }
            if (status != kOk) {
                break;
            }
            changes.emplace_back(entry.key, std::move(stored));
        }
    }
    if (status == kOk) {
        for (auto& [key, value] : changes) {
            propsOf(stateOf(world), *player)[key] = std::move(value);
        }
    }
    c.ret(rec.done(status == kErrAbsent ? kErrInvalid : status));
}

void playerGet(void* u, Call& c) {
    World& world = worldOf(u);
    Rec rec("hud_player_get");
    rec.i(c.i32(0)).cap(c.i32(2));
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    Value::Map map;
    for (const std::string_view key : kPlayerProperties) {
        map.push_back({std::string{key}, propsOf(stateOf(world), *player).at(std::string{key})});
    }
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2), Value{std::move(map)}));
}

void tabEntry(void* u, Call& c) {
    World& world = worldOf(u);
    Rec rec("hud_tab_entry");
    rec.i(c.i32(0)).i(c.i32(1)).i(c.i32(2));
    Player* viewer = nullptr;
    std::int32_t status = checkPlayer(world, c.i32(0), viewer);
    if (status == kOk) {
        const std::int32_t entity = c.i32(1);
        const Player* listedPlayer = world.playerByHandle(entity - kPlayerEntityBase);
        if (listedPlayer == nullptr || !listedPlayer->online) {
            status = kErrInvalid;
        }
    }
    c.ret(rec.done(status));
}

[[nodiscard]] std::int32_t readContent(const World& world, const Value& spec, Value& out) {
    std::optional<std::string> title;
    std::int32_t status = takeString(spec, "title", title);
    Value::Map map;
    if (status == kOk && title) {
        std::string rendered;
        status = renderPlain(world, *title, rendered);
        map.push_back({"title", Value{rendered}});
    }
    const Value* lines = spec.find("lines");
    if (status == kOk && lines != nullptr) {
        const Value::List* list = lines->asList();
        if (list == nullptr) {
            return kErrInvalid;
        }
        Value::List rendered;
        for (const Value& item : *list) {
            const std::string* text = item.asString();
            std::string body;
            if (text == nullptr || renderPlain(world, *text, body) != kOk) {
                return kErrInvalid;
            }
            rendered.emplace_back(body);
        }
        map.push_back({"lines", Value{std::move(rendered)}});
    }
    out = Value{std::move(map)};
    return status == kErrAbsent ? kErrInvalid : status;
}

void surfaceClaim(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> spec = readSpec(memory, c.i32(3), c.i32(4));
    Rec rec("hud_surface_claim");
    rec.str(name).i(c.i32(2)).value(spec).i(c.i32(5));
    const bool known = name && std::ranges::find(kSurfaces, *name) != kSurfaces.end();
    Value fixed;
    if (!known || !spec || c.i32(5) < 0 || readContent(world, *spec, fixed) != kOk) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    state.claims.push_back(
        Claim{.surface = std::string{*name}, .priority = c.i32(2), .token = c.i32(5), .fixed = std::move(fixed)});
    c.ret(rec.done(static_cast<std::int32_t>(state.claims.size())));
}

void surfaceUpdate(void* u, Call& c) {
    World& world = worldOf(u);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    Rec rec("hud_surface_update");
    rec.i(c.i32(0)).value(spec);
    Claim* claim = nullptr;
    std::int32_t status = findIn(stateOf(world).claims, c.i32(0), claim);
    Value fixed;
    if (status == kOk) {
        status = spec ? readContent(world, *spec, fixed) : kErrInvalid;
    }
    if (status == kOk) {
        claim->fixed = std::move(fixed);
    }
    c.ret(rec.done(status));
}

void surfaceRelease(void* u, Call& c) {
    Claim* claim = nullptr;
    const std::int32_t status = findIn(stateOf(worldOf(u)).claims, c.i32(0), claim);
    if (status == kOk) {
        claim->alive = false;
    }
    c.ret(Rec("hud_surface_release").i(c.i32(0)).done(status));
}

void surfaceAnswer(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    Rec rec("hud_surface_answer");
    rec.i(c.i32(0)).value(spec);
    Context* context = nullptr;
    std::int32_t status = world.checkContext(c.i32(0), ContextKind::Completion, context);
    if (status == kOk && !state.answering) {
        status = kErrInvalid;
    }
    Value content;
    if (status == kOk) {
        status = spec ? readContent(world, *spec, content) : kErrInvalid;
    }
    if (status == kOk) {
        state.answer = std::move(content);
    }
    c.ret(rec.done(status));
}

[[nodiscard]] bool emptyContent(const Value& content) {
    const Value* title = content.find("title");
    const Value* lines = content.find("lines");
    const bool noTitle = title == nullptr || title->asString()->empty();
    const bool noLines = lines == nullptr || lines->asList()->empty();
    return noTitle && noLines;
}

[[nodiscard]] std::optional<Value> renderClaim(World& world, const Claim& claim, const Player& player) {
    State& state = stateOf(world);
    if (claim.token == 0) {
        if (!claim.fixed || emptyContent(*claim.fixed)) {
            return std::nullopt;
        }
        return claim.fixed;
    }
    state.answering = true;
    state.answer.reset();
    Value::Map map;
    map.push_back({"surface", Value{claim.surface}});
    map.push_back({"player", Value{player.name}});
    const std::int32_t ran = deliverValue(world, claim.token, Value{std::move(map)});
    state.answering = false;
    std::optional<Value> answer = std::move(state.answer);
    state.answer.reset();
    if (ran != kOk || !answer || emptyContent(*answer)) {
        return std::nullopt;
    }
    return answer;
}

bool redraw(World& world, std::string_view rest) {
    if (!rest.empty()) {
        return false;
    }
    State& state = stateOf(world);
    for (const std::string_view surface : kSurfaces) {
        std::vector<const Claim*> candidates;
        for (const Claim& claim : state.claims) {
            if (claim.alive && claim.surface == surface) {
                candidates.push_back(&claim);
            }
        }
        if (candidates.empty()) {
            continue;
        }
        std::ranges::stable_sort(
            candidates, [](const Claim* left, const Claim* right) { return left->priority > right->priority; });
        for (const Player& player : world.players) {
            if (!player.online) {
                continue;
            }
            std::string drawn = "none";
            for (const Claim* claim : candidates) {
                if (const std::optional<Value> content = renderClaim(world, *claim, player)) {
                    drawn = canonical(*content);
                    break;
                }
            }
            emitLine("world surface " + std::string{surface} + " " + player.name + " " + drawn);
        }
    }
    return true;
}

const ScenarioVerbRegistrar kRedrawVerb{"redraw", redraw};
} // namespace
} // namespace hud

void addHudBarsImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"hud_bossbar_create", hud::bossbarCreate, user},
                              {"hud_bossbar_attach", hud::bossbarAttach, user},
                              {"hud_bossbar_update", hud::bossbarUpdate, user},
                              {"hud_bossbar_remove", hud::bossbarRemove, user},
                              {"hud_bossbar_player_add", hud::bossbarPlayerAdd, user},
                              {"hud_bossbar_player_remove", hud::bossbarPlayerRemove, user},
                              {"hud_bossbar_players", hud::bossbarPlayers, user},
                              {"hud_bossbar_get", hud::bossbarGet, user},
                              {"hud_bossbar_kept", hud::bossbarKept, user},
                              {"hud_player_set", hud::playerSet, user},
                              {"hud_player_get", hud::playerGet, user},
                              {"hud_tab_entry", hud::tabEntry, user},
                              {"hud_surface_claim", hud::surfaceClaim, user},
                              {"hud_surface_update", hud::surfaceUpdate, user},
                              {"hud_surface_release", hud::surfaceRelease, user},
                              {"hud_surface_answer", hud::surfaceAnswer, user},
                          });
}

} // namespace basalt::wire::wasm::trace
