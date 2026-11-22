#include "HostWorlds.hpp"

#include <algorithm>

namespace basalt::wire::wasm::trace::worlds {
namespace {
constexpr std::array<std::string_view, 44> kOps{"save",
                                                "use_item",
                                                "survives",
                                                "bone_meal",
                                                "break_block",
                                                "drops",
                                                "signal",
                                                "own_signal",
                                                "analog_signal",
                                                "fluid",
                                                "fluid_flow",
                                                "highest",
                                                "random_tick",
                                                "grow_tree",
                                                "place_tree",
                                                "place_feature",
                                                "feature_ids",
                                                "pois",
                                                "climate",
                                                "humidity",
                                                "temperature",
                                                "beacon_scan",
                                                "lightning_target",
                                                "raids",
                                                "raid_control",
                                                "raid_bar",
                                                "set_raid",
                                                "sculk",
                                                "hive",
                                                "vault",
                                                "creaking_heart",
                                                "dragon_fight",
                                                "dragon_fight_action",
                                                "dragon_fight_bar",
                                                "place_end_gateway",
                                                "random_sequence",
                                                "inhabited_time",
                                                "force_chunk",
                                                "move_to_time_marker",
                                                "chunk_holders",
                                                "break_speed",
                                                "structures",
                                                "locate_structure",
                                                "locate_biome"};

constexpr std::array<std::string_view, 26> kTables{"blocks",
                                                   "block_properties",
                                                   "state_flags",
                                                   "block_hardness",
                                                   "block_info",
                                                   "block_shape",
                                                   "block_outline",
                                                   "block_visual",
                                                   "block_entity_types",
                                                   "block_sounds",
                                                   "block_instrument",
                                                   "tool_facts",
                                                   "transform_state",
                                                   "poi_type",
                                                   "poi_tickets",
                                                   "game_rules",
                                                   "game_event_frequency",
                                                   "data_version",
                                                   "fix_nbt",
                                                   "fix_name",
                                                   "biomes",
                                                   "items",
                                                   "entity_types",
                                                   "tags",
                                                   "registry",
                                                   "registry_data"};

[[nodiscard]] std::optional<Value> runOp(SimWorld& sw, std::string_view op, const Value& args) {
    if (op == "highest") {
        std::int32_t top = kMinY - 1;
        for (const auto& [at, state] : sw.blocks) {
            if (at[0] == intArg(args, "x") && at[2] == intArg(args, "z")) {
                top = std::max(top, at[1]);
            }
        }
        return Value{top};
    }
    if (op == "survives") {
        return Value{intArg(args, "state") != 4};
    }
    if (op == "fluid") {
        return Value{Value::Map{
            {"fluid", Value{"empty"}}, {"source", Value{false}}, {"level", Value{0}}, {"falling", Value{false}}}};
    }
    if (op == "chunk_holders") {
        Value::List names;
        for (const auto& [tag, chunks] : sw.held) {
            if (chunks.contains({intArg(args, "chunk_x"), intArg(args, "chunk_z")})) {
                names.emplace_back(tag);
            }
        }
        return Value{std::move(names)};
    }
    if (op == "random_sequence") {
        return Value{std::int64_t{42}};
    }
    if (op == "pois") {
        return Value{Value::List{}};
    }
    if (op == "save") {
        return Value{};
    }
    return std::nullopt;
}

void worldCall(void* u, Call& c) {
    Traced t(u, c, "world_call", "wsvo");
    const auto op = c.memory().string(c.i32(1), c.i32(2));
    const std::expected<Value, std::int32_t> args = c.i32(4) == 0
                                                        ? std::expected<Value, std::int32_t>{Value{Value::Map{}}}
                                                        : readValue(c.memory(), c.i32(3), c.i32(4));
    if (t.status != kOk || !op) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    if (*op == "ops") {
        Value::List names;
        for (const std::string_view name : kOps) {
            names.emplace_back(std::string{name});
        }
        t.endValue(c.i32(5), c.i32(6), Value{std::move(names)});
        return;
    }
    if (!std::ranges::contains(kOps, *op)) {
        t.end(kErrAbsent);
        return;
    }
    if (!args || args->asMap() == nullptr) {
        t.end(kErrInvalid);
        return;
    }
    const std::optional<Value> answer = runOp(*t.sim, *op, *args);
    t.endValue(c.i32(5), c.i32(6), answer ? *answer : Value{});
}

void worldsData(void* u, Call& c) {
    Traced t(u, c, "worlds_data", "svo");
    const auto query = c.memory().string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> args = c.i32(3) == 0
                                                        ? std::expected<Value, std::int32_t>{Value{Value::Map{}}}
                                                        : readValue(c.memory(), c.i32(2), c.i32(3));
    if (!query || !args) {
        t.end(kErrInvalid);
        return;
    }
    if (*query == "tables") {
        Value::List names;
        for (const std::string_view name : kTables) {
            names.emplace_back(std::string{name});
        }
        t.endValue(c.i32(4), c.i32(5), Value{std::move(names)});
        return;
    }
    if (!std::ranges::contains(kTables, *query)) {
        t.end(kErrAbsent);
        return;
    }
    if (*query == "blocks") {
        t.endValue(
            c.i32(4), c.i32(5),
            Value{Value::List{
                Value{Value::Map{{"id", Value{"minecraft:air"}}, {"first_state", Value{0}}, {"state_count", Value{1}}}},
                Value{Value::Map{
                    {"id", Value{"minecraft:stone"}}, {"first_state", Value{1}}, {"state_count", Value{1}}}}}});
    } else if (*query == "data_version") {
        t.endValue(c.i32(4), c.i32(5), Value{5023});
    } else if (*query == "biomes") {
        t.endValue(c.i32(4), c.i32(5),
                   Value{Value::List{Value{"minecraft:plains"}, Value{"minecraft:desert"}, Value{"minecraft:forest"}}});
    } else if (*query == "state_flags") {
        t.endValue(c.i32(4), c.i32(5), Value{intArg(*args, "state") == 0 ? 0 : 3});
    } else if (*query == "game_rules") {
        t.endValue(c.i32(4), c.i32(5),
                   Value{Value::List{Value{Value::Map{
                       {"id", Value{"doDaylightCycle"}}, {"type", Value{"boolean"}}, {"default", Value{"true"}}}}}});
    } else {
        t.endValue(c.i32(4), c.i32(5), Value{});
    }
}

void stateIdOf(void* u, Call& c) {
    Traced t(u, c, "block_state_id", "s");
    const auto text = c.memory().string(c.i32(0), c.i32(1));
    const std::int32_t id = text ? stateId(*text) : -1;
    t.end(id < 0 ? kErrAbsent : id);
}

void stateTextOf(void* u, Call& c) {
    Traced t(u, c, "block_state_text", "io");
    const std::string text = stateText(c.i32(0));
    if (text.empty()) {
        t.end(kErrAbsent);
        return;
    }
    t.endText(c.i32(1), c.i32(2), text);
}

[[nodiscard]] std::vector<std::uint8_t>* templateOf(World& world, std::string_view id) {
    const auto found = sim(world).templates.find(std::string{id});
    return found == sim(world).templates.end() ? nullptr : &found->second;
}

void structureList(void* u, Call& c) {
    Traced t(u, c, "structure_list", "o");
    Value::List ids;
    for (const auto& [id, nbt] : sim(t.world).templates) {
        ids.emplace_back(id);
    }
    t.endValue(c.i32(0), c.i32(1), Value{std::move(ids)});
}

void structureGet(void* u, Call& c) {
    Traced t(u, c, "structure_get", "so");
    const auto id = c.memory().string(c.i32(0), c.i32(1));
    const std::vector<std::uint8_t>* found = id ? templateOf(t.world, *id) : nullptr;
    if (found == nullptr) {
        t.end(id ? kErrAbsent : kErrInvalid);
        return;
    }
    t.endBytes(c.i32(2), c.i32(3), *found);
}

void structurePut(void* u, Call& c) {
    Traced t(u, c, "structure_put", "syi");
    const auto id = c.memory().string(c.i32(0), c.i32(1));
    const auto nbt = t.bytes(c.i32(2), c.i32(3));
    if (!id || id->empty() || !nbt || nbt->size() < 4) {
        t.end(kErrInvalid);
        return;
    }
    sim(t.world).templates[std::string{*id}] = *nbt;
    t.end(kOk);
}

void structureRemove(void* u, Call& c) {
    Traced t(u, c, "structure_remove", "si");
    const auto id = c.memory().string(c.i32(0), c.i32(1));
    t.end(!id ? kErrInvalid : (sim(t.world).templates.erase(std::string{*id}) > 0 ? kOk : kErrAbsent));
}

void structureFile(void* u, Call& c) {
    Traced t(u, c, "structure_file", "so");
    const auto id = c.memory().string(c.i32(0), c.i32(1));
    if (!id) {
        t.end(kErrInvalid);
        return;
    }
    if (templateOf(t.world, *id) == nullptr) {
        t.end(kErrAbsent);
        return;
    }
    t.endText(c.i32(2), c.i32(3), "generated/" + std::string{*id});
}

void datafixCall(void* u, Call& c) {
    Traced t(u, c, "datafix_call", "svo");
    const auto op = c.memory().string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> args = c.i32(3) == 0
                                                        ? std::expected<Value, std::int32_t>{Value{Value::Map{}}}
                                                        : readValue(c.memory(), c.i32(2), c.i32(3));
    if (!op || !args || args->asMap() == nullptr) {
        t.end(kErrInvalid);
        return;
    }
    const Value* typeField = args->find("type");
    const std::string type = typeField != nullptr && typeField->asString() != nullptr ? *typeField->asString() : "";
    const bool known = type == "chunk" || type == "player";
    if (*op == "types") {
        t.endValue(c.i32(4), c.i32(5), Value{Value::List{Value{"chunk"}, Value{"player"}}});
    } else if (*op == "supports") {
        t.endValue(c.i32(4), c.i32(5), Value{known});
    } else if (*op == "legacy_state" || *op == "modern_state") {
        t.endValue(c.i32(4), c.i32(5),
                   Value{Value::Map{{"name", Value{"minecraft:stone"}}, {"properties", Value{Value::Map{}}}}});
    } else if (!known) {
        t.end(kErrAbsent);
    } else if (*op == "steps") {
        t.endValue(c.i32(4), c.i32(5),
                   Value{Value::List{Value{
                       Value::Map{{"version", Value{1343}}, {"sub_version", Value{0}}, {"name", Value{"fix"}}}}}});
    } else if (*op == "missing") {
        t.endValue(c.i32(4), c.i32(5), Value{""});
    } else if (*op == "upgrade" || *op == "step") {
        const Value* nbt = args->find("nbt");
        if (nbt == nullptr || nbt->asBytes() == nullptr) {
            t.end(kErrInvalid);
        } else {
            t.endBytes(c.i32(4), c.i32(5), nbt->asBytes()->data);
        }
    } else {
        t.end(kErrAbsent);
    }
}

void structureCapture(void* u, Call& c) {
    Traced t(u, c, "structure_capture", "wiiiiiiio");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    std::int32_t nonAir = 0;
    for (std::int32_t y = 0; y < c.i32(5); ++y) {
        for (std::int32_t z = 0; z < c.i32(6); ++z) {
            for (std::int32_t x = 0; x < c.i32(4); ++x) {
                nonAir += stateAt(*t.sim, {c.i32(1) + x, c.i32(2) + y, c.i32(3) + z}) != 0 ? 1 : 0;
            }
        }
    }
    if (!loaded(*t.sim, c.i32(1), c.i32(3))) {
        t.end(kErrAbsent);
        return;
    }
    const std::vector<std::uint8_t> nbt{static_cast<std::uint8_t>(c.i32(4)), static_cast<std::uint8_t>(c.i32(5)),
                                        static_cast<std::uint8_t>(c.i32(6)), static_cast<std::uint8_t>(nonAir)};
    t.endBytes(c.i32(8), c.i32(9), nbt);
}

void structureScan(void* u, Call& c) {
    Traced t(u, c, "structure_scan", "yvo");
    const auto nbt = t.bytes(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(2), c.i32(3));
    if (!nbt || nbt->size() < 4 || !spec || spec->asMap() == nullptr) {
        t.end(kErrInvalid);
        return;
    }
    Value::List blocks;
    blocks.emplace_back(Value::Map{{"x", Value{intArg(*spec, "x")}},
                                   {"y", Value{intArg(*spec, "y")}},
                                   {"z", Value{intArg(*spec, "z")}},
                                   {"state", Value{static_cast<std::int32_t>((*nbt)[3])}},
                                   {"nbt", Value{Blob{}}}});
    const Value result{Value::Map{{"size", Value{Value::List{Value{static_cast<std::int32_t>((*nbt)[0])},
                                                             Value{static_cast<std::int32_t>((*nbt)[1])},
                                                             Value{static_cast<std::int32_t>((*nbt)[2])}}}},
                                  {"blocks", Value{std::move(blocks)}},
                                  {"jigsaws", Value{Value::List{}}}}};
    t.endValue(c.i32(4), c.i32(5), result);
}

void structurePlace(void* u, Call& c) {
    Traced t(u, c, "structure_place", "yv");
    const auto nbt = t.bytes(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(2), c.i32(3));
    if (!nbt || nbt->size() < 4 || !spec || spec->asMap() == nullptr) {
        t.end(kErrInvalid);
        return;
    }
    const Value* name = spec->find("world");
    SimWorld* target = nullptr;
    for (SimWorld& sw : sim(t.world).worlds) {
        target =
            sw.alive && name != nullptr && name->asString() != nullptr && sw.name == *name->asString() ? &sw : target;
    }
    if (target == nullptr) {
        t.end(kErrInvalid);
        return;
    }
    const Pos at{intArg(*spec, "x"), intArg(*spec, "y"), intArg(*spec, "z")};
    if (!loaded(*target, at[0], at[2])) {
        t.end(kErrAbsent);
        return;
    }
    if ((*nbt)[3] != 0) {
        target->blocks[at] = (*nbt)[3];
    }
    t.end(kOk);
}

void noiseQuery(void* u, Call& c) {
    Traced t(u, c, "noise_query", "vo");
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(0), c.i32(1));
    if (!spec || spec->asMap() == nullptr) {
        t.end(kErrInvalid);
        return;
    }
    const Value* op = spec->find("op");
    const Value* settings = spec->find("settings");
    const std::string name = op != nullptr && op->asString() != nullptr ? *op->asString() : std::string{};
    const std::string where =
        settings != nullptr && settings->asString() != nullptr ? *settings->asString() : "minecraft:overworld";
    if (where != "minecraft:overworld" && where != "minecraft:nether") {
        t.end(kErrAbsent);
        return;
    }
    if (name == "shape") {
        t.endValue(c.i32(2), c.i32(3),
                   Value{Value::Map{{"min_y", Value{kMinY}},
                                    {"height", Value{kHeight}},
                                    {"sea_level", Value{63}},
                                    {"default_block", Value{1}},
                                    {"default_fluid", Value{0}},
                                    {"legacy_random_source", Value{false}},
                                    {"disable_mob_generation", Value{false}}}});
    } else if (name == "density" || name == "noise") {
        t.endValue(c.i32(2), c.i32(3), Value{realArg(*spec, "x") + realArg(*spec, "y") + realArg(*spec, "z")});
    } else if (name == "height") {
        t.endValue(c.i32(2), c.i32(3), Value{63});
    } else {
        t.end(kErrAbsent);
    }
}

void noiseCreate(void* u, Call& c) {
    Traced t(u, c, "noise_create", "v");
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(0), c.i32(1));
    if (!spec || spec->asMap() == nullptr || (spec->find("id") == nullptr && spec->find("parameters") == nullptr)) {
        t.end(kErrInvalid);
        return;
    }
    Sim& all = sim(t.world);
    all.noises.push_back(true);
    t.end(static_cast<std::int32_t>(all.noises.size()));
}

[[nodiscard]] bool liveNoise(Sim& all, std::int32_t handle) {
    return handle >= 1 && static_cast<std::size_t>(handle) <= all.noises.size() &&
           all.noises[static_cast<std::size_t>(handle - 1)];
}

void noiseSample(void* u, Call& c) {
    Traced t(u, c, "noise_sample", "idddx");
    if (!liveNoise(sim(t.world), c.i32(0))) {
        t.end(c.i32(0) <= 0 ? kErrInvalid : kErrStale);
        return;
    }
    const double value = c.f64(1) * 0.5 + c.f64(2) * 0.25 + c.f64(3) * 0.125;
    if (!t.memory.write(c.i32(4), value)) {
        t.end(kErrInvalid);
        return;
    }
    c.ret(t.rec.done(kOk, "out=" + number(value)));
}

void noiseInfo(void* u, Call& c) {
    Traced t(u, c, "noise_info", "io");
    if (!liveNoise(sim(t.world), c.i32(0))) {
        t.end(c.i32(0) <= 0 ? kErrInvalid : kErrStale);
        return;
    }
    t.endValue(c.i32(1), c.i32(2), Value{Value::List{Value{-1.0}, Value{1.0}}});
}

void noiseRelease(void* u, Call& c) {
    Traced t(u, c, "noise_release", "i");
    Sim& all = sim(t.world);
    if (!liveNoise(all, c.i32(0))) {
        t.end(c.i32(0) <= 0 ? kErrInvalid : kErrStale);
        return;
    }
    all.noises[static_cast<std::size_t>(c.i32(0) - 1)] = false;
    t.end(kOk);
}
} // namespace
} // namespace basalt::wire::wasm::trace::worlds

namespace basalt::wire::wasm::trace {
void addWorldsMoreImports(World& world, std::vector<HostImport>& out) {
    using namespace worlds;
    World* user = &world;
    out.insert(out.end(), {
                              {"world_call", worldCall, user},
                              {"worlds_data", worldsData, user},
                              {"block_state_id", stateIdOf, user},
                              {"block_state_text", stateTextOf, user},
                              {"structure_list", structureList, user},
                              {"structure_get", structureGet, user},
                              {"structure_put", structurePut, user},
                              {"structure_remove", structureRemove, user},
                              {"structure_file", structureFile, user},
                              {"datafix_call", datafixCall, user},
                              {"structure_capture", structureCapture, user},
                              {"structure_scan", structureScan, user},
                              {"structure_place", structurePlace, user},
                              {"noise_query", noiseQuery, user},
                              {"noise_create", noiseCreate, user},
                              {"noise_sample", noiseSample, user},
                              {"noise_info", noiseInfo, user},
                              {"noise_release", noiseRelease, user},
                          });
}
} // namespace basalt::wire::wasm::trace
