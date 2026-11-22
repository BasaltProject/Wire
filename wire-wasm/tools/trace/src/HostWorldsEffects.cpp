#include "HostWorlds.hpp"

#include <algorithm>

namespace basalt::wire::wasm::trace::worlds {
namespace {
constexpr std::array<std::string_view, 11> kCategories{"master",  "music",  "record",  "weather", "block", "hostile",
                                                       "neutral", "player", "ambient", "voice",   "ui"};
constexpr std::array<std::string_view, 19> kViewOperations{
    "sound_at",        "sound_entity",      "stop_sound",         "particle_at",  "world_event",
    "set_player_time", "reset_player_time", "set_player_weather", "block_change", "block_damage",
    "sign_text",       "open_sign",         "game_event",         "border_set",   "border_reset",
    "block_entity",    "post_effects",      "set_post_effects",   "entity_event"};
constexpr std::array<std::string_view, 5> kViewProperties{"player_time", "player_time_relative", "player_weather",
                                                          "view_distance", "simulation_distance"};

void sound(void* u, Call& c) {
    Traced t(u, c, "world_sound", "wssdddffl");
    const auto id = c.memory().string(c.i32(1), c.i32(2));
    const auto category = c.memory().string(c.i32(3), c.i32(4));
    if (t.status != kOk || !id || id->empty() || !category) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    const std::string_view named = category->empty() ? "master" : *category;
    if (!std::ranges::contains(kCategories, named)) {
        t.end(kErrInvalid);
        return;
    }
    emitLine("world sound " + t.sim->name + " " + quote(*id) + " " + std::string{named});
    t.end(kOk);
}

void particle(void* u, Call& c) {
    Traced t(u, c, "world_particle", "wv");
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(1), c.i32(2));
    if (t.status != kOk || !spec || spec->asMap() == nullptr) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    const Value* type = spec->find("type");
    if (type == nullptr || type->asString() == nullptr || type->asString()->empty()) {
        t.end(kErrInvalid);
        return;
    }
    emitLine("world particle " + t.sim->name + " " + quote(*type->asString()) +
             " count=" + std::to_string(intArg(*spec, "count", 1)));
    t.end(kOk);
}

void levelEvent(void* u, Call& c) {
    Traced t(u, c, "world_level_event", "wiiiiii");
    if (t.status == kOk) {
        emitLine("world level-event " + t.sim->name + " " + std::to_string(c.i32(1)) +
                 " global=" + std::to_string((c.i32(6) & 64) != 0 ? 1 : 0));
    }
    t.end(t.status);
}

void blockEvent(void* u, Call& c) {
    Traced t(u, c, "world_block_event", "wiiiii");
    if (t.status == kOk) {
        emitLine("world block-event " + t.sim->name + " " + std::to_string(c.i32(4)) + " " + std::to_string(c.i32(5)));
    }
    t.end(t.status);
}

void explode(void* u, Call& c) {
    Traced t(u, c, "world_explode", "wdddfii");
    if (t.status == kOk) {
        emitLine("world explode " + t.sim->name + " power=" + number(c.f32(4)) +
                 " break=" + std::to_string((c.i32(5) & 128) != 0 ? 1 : 0));
    }
    t.end(t.status != kOk ? t.status : 1);
}

void lightning(void* u, Call& c) {
    Traced t(u, c, "world_lightning", "wddd");
    static std::int32_t bolt = 100;
    t.end(t.status != kOk ? t.status : ++bolt);
}

void gameEvent(void* u, Call& c) {
    Traced t(u, c, "world_game_event", "wsddds");
    const auto event = c.memory().string(c.i32(1), c.i32(2));
    if (t.status != kOk || !event || event->empty()) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    emitLine("world game-event " + t.sim->name + " " + quote(*event));
    t.end(kOk);
}

void playerEffect(void* u, Call& c) {
    Traced t(u, c, "world_player_effect", "isvo");
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(t.world, c.i32(0), player);
    const auto op = c.memory().string(c.i32(1), c.i32(2));
    const std::expected<Value, std::int32_t> args = c.i32(4) == 0
                                                        ? std::expected<Value, std::int32_t>{Value{Value::List{}}}
                                                        : readValue(c.memory(), c.i32(3), c.i32(4));
    if (status != kOk || !op || !args || args->asList() == nullptr) {
        t.end(status != kOk ? status : kErrInvalid);
        return;
    }
    const bool operation = std::ranges::contains(kViewOperations, *op);
    if (!operation && !std::ranges::contains(kViewProperties, *op)) {
        t.end(kErrAbsent);
        return;
    }
    emitLine("world view " + player->name + " " + std::string{*op} + " args=" + canonical(*args));
    t.endValue(c.i32(5), c.i32(6), operation || !args->asList()->empty() ? Value{} : Value{0});
}

void borderGet(void* u, Call& c) {
    Traced t(u, c, "world_border_get", "wo");
    if (t.status != kOk) {
        t.end(t.status);
        return;
    }
    Value::Map map;
    for (const auto& [key, value] : t.sim->border) {
        map.push_back(MapEntry{key, key == "lerp_ticks" || key == "warning_distance" || key == "warning_time"
                                        ? Value{static_cast<std::int64_t>(value)}
                                        : Value{value}});
    }
    t.endValue(c.i32(1), c.i32(2), Value{std::move(map)});
}

void borderSet(void* u, Call& c) {
    Traced t(u, c, "world_border_set", "wv");
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(1), c.i32(2));
    if (t.status != kOk || !spec || spec->asMap() == nullptr) {
        t.end(t.status != kOk ? t.status : kErrInvalid);
        return;
    }
    for (auto& [key, value] : t.sim->border) {
        value = realArg(*spec, key, value);
    }
    if (t.sim->border["size"] < 0.0 || realArg(*spec, "seconds") < 0.0) {
        t.end(kErrInvalid);
        return;
    }
    t.end(kOk);
}
} // namespace
} // namespace basalt::wire::wasm::trace::worlds

namespace basalt::wire::wasm::trace {
void addWorldsEffectsImports(World& world, std::vector<HostImport>& out) {
    using namespace worlds;
    World* user = &world;
    out.insert(out.end(), {
                              {"world_sound", sound, user},
                              {"world_particle", particle, user},
                              {"world_level_event", levelEvent, user},
                              {"world_block_event", blockEvent, user},
                              {"world_explode", explode, user},
                              {"world_lightning", lightning, user},
                              {"world_game_event", gameEvent, user},
                              {"world_player_effect", playerEffect, user},
                              {"world_border_get", borderGet, user},
                              {"world_border_set", borderSet, user},
                          });
}
} // namespace basalt::wire::wasm::trace
