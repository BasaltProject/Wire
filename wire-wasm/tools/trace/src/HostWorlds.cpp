#include "HostWorlds.hpp"

#include <algorithm>

namespace basalt::wire::wasm::trace::worlds {
namespace {
const std::vector<std::string> kStates{"minecraft:air",
                                       "minecraft:stone",
                                       "minecraft:dirt",
                                       "minecraft:oak_planks",
                                       "minecraft:oak_stairs[facing=east,half=top]",
                                       "minecraft:glowstone",
                                       "minecraft:chest[facing=north]"};

struct Property {
    std::string_view name;
    std::string_view type;
    bool writable;
};

constexpr std::array kProperties{
    Property{"name", "string", false},
    Property{"dimension", "string", false},
    Property{"seed", "int", false},
    Property{"min_height", "int", false},
    Property{"height", "int", false},
    Property{"sea_level", "int", false},
    Property{"view_distance", "int", true},
    Property{"time", "int", false},
    Property{"day_time", "int", true},
    Property{"raining", "bool", false},
    Property{"thundering", "bool", false},
    Property{"difficulty", "string", true},
    Property{"difficulty_locked", "bool", true},
    Property{"spawn", "list", true},
    Property{"folder", "string", false},
    Property{"player_count", "int", false},
    Property{"block_entity_count", "int", false},
    Property{"tickable_block_entity_count", "int", false},
    Property{"block_version", "int", false},
    Property{"forced_chunks", "list", false},
    Property{"weather_times", "map", true},
    Property{"clock", "map", false},
    Property{"spawn_rules", "map", true},
    Property{"rules", "list", false},
    Property{"plugin_data", "bytes", true},
};

[[nodiscard]] Value ints(std::span<const std::int32_t> values) {
    Value::List out;
    for (const std::int32_t value : values) {
        out.emplace_back(value);
    }
    return Value{std::move(out)};
}

[[nodiscard]] std::int32_t readProperty(World& world, const SimWorld& sw, std::string_view key, Value& out) {
    if (key == "properties") {
        Value::List rows;
        for (const Property& property : kProperties) {
            rows.emplace_back(Value::Map{{"name", Value{std::string{property.name}}},
                                         {"type", Value{std::string{property.type}}},
                                         {"writable", Value{property.writable}}});
        }
        out = Value{std::move(rows)};
    } else if (key == "name") {
        out = Value{sw.name};
    } else if (key == "dimension") {
        out = Value{sw.dimension};
    } else if (key == "seed") {
        out = Value{sw.seed};
    } else if (key == "min_height") {
        out = Value{kMinY};
    } else if (key == "height") {
        out = Value{kHeight};
    } else if (key == "sea_level") {
        out = Value{63};
    } else if (key == "view_distance") {
        out = Value{sw.viewDistance};
    } else if (key == "time") {
        out = Value{world.now};
    } else if (key == "day_time") {
        out = Value{sw.dayTime};
    } else if (key == "raining") {
        out = Value{sw.raining};
    } else if (key == "thundering") {
        out = Value{sw.thundering};
    } else if (key == "difficulty") {
        out = Value{sw.difficulty};
    } else if (key == "difficulty_locked") {
        out = Value{sw.locked};
    } else if (key == "spawn") {
        out = ints(sw.spawn);
    } else if (key == "folder") {
        out = Value{"worlds/" + sw.name};
    } else if (key == "player_count") {
        out = Value{
            static_cast<std::int64_t>(std::ranges::count_if(world.players, [](const Player& p) { return p.online; }))};
    } else if (key == "block_entity_count") {
        out = Value{static_cast<std::int64_t>(sw.blockEntities.size())};
    } else if (key == "tickable_block_entity_count" || key == "block_version") {
        out = Value{0};
    } else if (key == "forced_chunks") {
        out = Value{Value::List{}};
    } else if (key == "weather_times") {
        out = Value{Value::Map{{"rain", Value{sw.weatherTimes[0]}},
                               {"thunder", Value{sw.weatherTimes[1]}},
                               {"clear", Value{sw.weatherTimes[2]}}}};
    } else if (key == "clock") {
        out = Value{Value::Map{{"total_ticks", Value{world.now}},
                               {"rate", Value{1.0}},
                               {"paused", Value{false}},
                               {"markers", Value{Value::List{}}}}};
    } else if (key == "spawn_rules") {
        out = Value{Value::Map{{"allow_monsters", Value{true}}, {"allow_animals", Value{true}}}};
    } else if (key == "rules") {
        Value::List names;
        for (const auto& [name, text] : sw.rules) {
            names.emplace_back(name);
        }
        out = Value{std::move(names)};
    } else if (key == "plugin_data") {
        out = Value{Blob{sw.pluginData}};
    } else if (key == "tunable.void_damage_amount") {
        out = Value{4.0};
    } else {
        return kErrAbsent;
    }
    return kOk;
}

[[nodiscard]] std::int32_t writeProperty(SimWorld& sw, std::string_view key, const Value& value) {
    if (key == "day_time" && value.asInt() != nullptr) {
        sw.dayTime = *value.asInt();
    } else if (key == "view_distance" && value.asInt() != nullptr) {
        if (*value.asInt() < 2 || *value.asInt() > 32) {
            return kErrInvalid;
        }
        sw.viewDistance = static_cast<std::int32_t>(*value.asInt());
    } else if (key == "difficulty" && value.asString() != nullptr) {
        if (*value.asString() != "peaceful" && *value.asString() != "easy" && *value.asString() != "normal" &&
            *value.asString() != "hard") {
            return kErrInvalid;
        }
        sw.difficulty = *value.asString();
    } else if (key == "difficulty_locked" && value.asBool() != nullptr) {
        sw.locked = *value.asBool();
    } else if (key == "spawn" && value.asList() != nullptr && value.asList()->size() == 3) {
        for (std::size_t at = 0; at < 3; ++at) {
            sw.spawn[at] = static_cast<std::int32_t>(intArg(Value{Value::Map{{"v", (*value.asList())[at]}}}, "v"));
        }
    } else if (key == "weather_times" && value.asMap() != nullptr) {
        sw.weatherTimes = {intArg(value, "rain", sw.weatherTimes[0]), intArg(value, "thunder", sw.weatherTimes[1]),
                           intArg(value, "clear", sw.weatherTimes[2])};
    } else if (key == "plugin_data" && value.asBytes() != nullptr) {
        sw.pluginData = value.asBytes()->data;
    } else if (key == "weather" && value.asMap() != nullptr) {
        const Value* rain = value.find("raining");
        const Value* thunder = value.find("thundering");
        sw.raining = rain != nullptr && rain->asBool() != nullptr && *rain->asBool();
        sw.thundering = thunder != nullptr && thunder->asBool() != nullptr && *thunder->asBool();
    } else if (key == "clock_rate" && value.asFloat() != nullptr) {
        if (*value.asFloat() < 0.00001 || *value.asFloat() > 1000.0) {
            return kErrInvalid;
        }
    } else {
        const auto known = std::ranges::find(kProperties, key, &Property::name);
        return known == kProperties.end() || !known->writable ? kErrAbsent : kErrInvalid;
    }
    return kOk;
}

void worldByName(void* u, Call& c) {
    Traced t(u, c, "world_by_name", "s");
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    std::int32_t handle = 0;
    for (std::size_t at = 0; at < sim(t.world).worlds.size() && name; ++at) {
        const SimWorld& sw = sim(t.world).worlds[at];
        handle = sw.alive && sw.name == *name ? static_cast<std::int32_t>(at + 1) : handle;
    }
    t.end(handle);
}

void worldDefault(void* u, Call& c) {
    Traced t(u, c, "world_default", "");
    t.end(1);
}

void worldList(void* u, Call& c) {
    Traced t(u, c, "world_list", "O");
    std::vector<std::int32_t> handles;
    for (std::size_t at = 0; at < sim(t.world).worlds.size(); ++at) {
        if (sim(t.world).worlds[at].alive) {
            handles.push_back(static_cast<std::int32_t>(at + 1));
        }
    }
    t.endInts(c.i32(0), c.i32(1), handles);
}

void worldName(void* u, Call& c) {
    Traced t(u, c, "world_name", "wo");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    t.endText(c.i32(1), c.i32(2), t.sim->name);
}

void worldGet(void* u, Call& c) {
    Traced t(u, c, "world_get", "wso");
    const auto key = c.memory().string(c.i32(1), c.i32(2));
    Value value;
    std::int32_t status = t.status;
    if (status == kOk) {
        status = key ? readProperty(t.world, *t.sim, *key, value) : kErrInvalid;
    }
    if (status != kOk) {
        t.end(status);
        return;
    }
    t.endValue(c.i32(3), c.i32(4), value);
}

void worldSet(void* u, Call& c) {
    Traced t(u, c, "world_set", "wsv");
    const auto key = c.memory().string(c.i32(1), c.i32(2));
    const std::expected<Value, std::int32_t> value = readValue(c.memory(), c.i32(3), c.i32(4));
    if (t.status != kOk) {
        t.end(t.status);
    } else {
        t.end(!key || !value ? kErrInvalid : writeProperty(*t.sim, *key, *value));
    }
}

void worldRuleGet(void* u, Call& c) {
    Traced t(u, c, "world_rule_get", "wso");
    const auto name = c.memory().string(c.i32(1), c.i32(2));
    if (t.status != kOk || !name) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    const auto rule = t.sim->rules.find(std::string{*name});
    if (rule == t.sim->rules.end()) {
        t.end(kErrAbsent);
        return;
    }
    t.endText(c.i32(3), c.i32(4), rule->second);
}

void worldRuleSet(void* u, Call& c) {
    Traced t(u, c, "world_rule_set", "wss");
    const auto name = c.memory().string(c.i32(1), c.i32(2));
    const auto value = c.memory().string(c.i32(3), c.i32(4));
    if (t.status != kOk || !name || !value) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    const auto rule = t.sim->rules.find(std::string{*name});
    if (rule == t.sim->rules.end()) {
        t.end(kErrAbsent);
        return;
    }
    const bool flag = rule->second == "true" || rule->second == "false";
    const bool fits =
        flag ? (*value == "true" || *value == "false")
             : !value->empty() && std::ranges::all_of(*value, [](char ch) { return ch >= '0' && ch <= '9'; });
    if (!fits) {
        t.end(kErrInvalid);
        return;
    }
    rule->second = std::string{*value};
    t.end(kOk);
}

void worldCreate(void* u, Call& c) {
    Traced t(u, c, "world_create", "v");
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(0), c.i32(1));
    if (!spec || spec->asMap() == nullptr) {
        t.end(kErrInvalid);
        return;
    }
    const Value* name = spec->find("name");
    const std::string text = name != nullptr && name->asString() != nullptr ? *name->asString() : std::string{};
    Sim& all = sim(t.world);
    const bool taken = std::ranges::any_of(all.worlds, [&](const SimWorld& w) { return w.alive && w.name == text; });
    if (text.empty() || taken) {
        t.end(kErrInvalid);
        return;
    }
    SimWorld made;
    made.name = text;
    const Value* dimension = spec->find("dimension");
    made.dimension =
        "minecraft:" +
        (dimension != nullptr && dimension->asString() != nullptr ? *dimension->asString() : std::string{"overworld"});
    made.seed =
        spec->find("seed") != nullptr && spec->find("seed")->asInt() != nullptr ? *spec->find("seed")->asInt() : 0;
    made.loaded.insert({0, 0});
    all.worlds.push_back(std::move(made));
    t.end(static_cast<std::int32_t>(all.worlds.size()));
}

void worldUnload(void* u, Call& c) {
    Traced t(u, c, "world_unload", "si");
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Sim& all = sim(t.world);
    for (std::size_t at = 0; at < all.worlds.size() && name; ++at) {
        SimWorld& sw = all.worlds[at];
        if (sw.alive && sw.name == *name) {
            if (at == 0) {
                t.end(kErrInvalid);
                return;
            }
            sw.alive = false;
            emitLine("world unload " + quote(*name) + " save=" + std::to_string((c.i32(2) & 16) != 0 ? 1 : 0));
            t.end(kOk);
            return;
        }
    }
    t.end(name ? kErrAbsent : kErrInvalid);
}

void worldDelete(void* u, Call& c) {
    Traced t(u, c, "world_delete", "s");
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Sim& all = sim(t.world);
    const bool known =
        name && std::ranges::any_of(all.worlds, [&](const SimWorld& w) { return !w.alive && w.name == *name; });
    if (known) {
        emitLine("world delete " + quote(*name));
    }
    t.end(known ? kOk : kErrAbsent);
}

void worldAt(void* u, Call& c) {
    Traced t(u, c, "world_at", "wddi");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    const std::int32_t request = beginRequest(t.world, c.i32(3));
    answerLater(t.world, request, Value{});
    t.end(request);
}
} // namespace

Sim& sim(World& world) {
    Sim& all = world.areaState<Sim>("worlds");
    if (all.worlds.empty()) {
        SimWorld first;
        first.name = "world";
        first.loaded = {{0, 0}, {-1, -1}, {1, 0}};
        all.worlds.push_back(std::move(first));
    }
    return all;
}

std::int32_t stateId(std::string_view text) {
    const auto found = std::ranges::find(kStates, text);
    return found == kStates.end() ? -1 : static_cast<std::int32_t>(found - kStates.begin());
}

std::string stateText(std::int32_t id) {
    return id >= 0 && static_cast<std::size_t>(id) < kStates.size() ? kStates[static_cast<std::size_t>(id)]
                                                                    : std::string{};
}

std::int32_t intArg(const Value& map, std::string_view key, std::int32_t fallback) {
    const Value* found = map.find(key);
    if (found != nullptr && found->asInt() != nullptr) {
        return static_cast<std::int32_t>(*found->asInt());
    }
    if (found != nullptr && found->asFloat() != nullptr) {
        return static_cast<std::int32_t>(*found->asFloat());
    }
    return fallback;
}

double realArg(const Value& map, std::string_view key, double fallback) {
    const Value* found = map.find(key);
    if (found != nullptr && found->asFloat() != nullptr) {
        return *found->asFloat();
    }
    if (found != nullptr && found->asInt() != nullptr) {
        return static_cast<double>(*found->asInt());
    }
    return fallback;
}

Traced::Traced(void* user, Call& call, std::string_view name, std::string_view kinds)
    : rec(name), c(call), world(worldOf(user)), memory(call.memory()) {
    std::size_t at = 0;
    bool first = true;
    for (const char kind : kinds) {
        switch (kind) {
        case 'w': {
            const std::int32_t handle = c.i32(at++);
            rec.i(handle);
            Sim& all = worlds::sim(world);
            if (handle <= 0 || static_cast<std::size_t>(handle) > all.worlds.size()) {
                status = kErrInvalid;
            } else if (!all.worlds[static_cast<std::size_t>(handle - 1)].alive) {
                status = kErrStale;
            } else {
                sim = &all.worlds[static_cast<std::size_t>(handle - 1)];
            }
            break;
        }
        case 'i':
            rec.i(c.i32(at++));
            break;
        case 'l':
            rec.i(c.i64(at++));
            break;
        case 'f':
            rec.f32(c.f32(at++));
            break;
        case 'd':
            rec.f64(c.f64(at++));
            break;
        case 's':
            rec.str(memory.string(c.i32(at), c.i32(at + 1)));
            at += 2;
            break;
        case 'v':
            rec.value(readValue(memory, c.i32(at), c.i32(at + 1)));
            at += 2;
            break;
        case 'y': {
            const auto data = bytes(c.i32(at), c.i32(at + 1));
            rec.raw(data ? hex(*data) : "<unreadable>");
            at += 2;
            break;
        }
        case 'x':
            ++at;
            break;
        case 'o':
        case 'O':
            rec.cap(c.i32(at + 1));
            at += 2;
            break;
        default:
            break;
        }
        first = false;
    }
    static_cast<void>(first);
}

} // namespace basalt::wire::wasm::trace::worlds

namespace basalt::wire::wasm::trace {
void addWorldsImports(World& world, std::vector<HostImport>& out) {
    using namespace worlds;
    World* user = &world;
    out.insert(out.end(), {
                              {"world_by_name", worldByName, user},
                              {"world_default", worldDefault, user},
                              {"world_list", worldList, user},
                              {"world_name", worldName, user},
                              {"world_get", worldGet, user},
                              {"world_set", worldSet, user},
                              {"world_rule_get", worldRuleGet, user},
                              {"world_rule_set", worldRuleSet, user},
                              {"world_create", worldCreate, user},
                              {"world_unload", worldUnload, user},
                              {"world_delete", worldDelete, user},
                              {"world_at", worldAt, user},
                          });
}
} // namespace basalt::wire::wasm::trace
