#pragma once

#include "HostApi.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <expected>

namespace basalt::wire::wasm::trace::entitiesSim {

inline constexpr std::string_view kWorld = "world";
inline constexpr std::int32_t kPlayerEntityBase = 100;

struct Goal {
    std::string key;
    std::int64_t priority = 0;
    std::string flags;
};

struct Effect {
    std::string id;
    std::int64_t ticks = 0;
    std::int64_t amplifier = 0;
};

struct Entity {
    std::int32_t id = 0;
    std::string uuid;
    std::string type;
    std::string name;
    std::array<double, 3> pos{};
    std::array<double, 3> velocity{};
    double yaw = 0.0;
    double pitch = 0.0;
    double health = 20.0;
    bool alive = true;
    bool player = false;
    std::int32_t playerHandle = 0;
    std::vector<std::pair<std::string, Value>> props;
    std::vector<Goal> goals;
    std::vector<Effect> effects;
};

struct Sim {
    std::vector<Entity> entities;
    std::vector<std::int32_t> handles;
    std::int32_t nextId = 1;
    std::int64_t limits[3] = {-1, -1, -1};
};

inline Sim& sim(World& world) { return world.areaState<Sim>("entities"); }

inline constexpr std::array<std::string_view, 3> kTypes{"minecraft:pig", "minecraft:zombie", "minecraft:item"};
inline constexpr std::array<std::string_view, 17> kBase{"id",  "type",  "world",  "uuid",  "alive",   "x",
                                                        "y",   "z",     "yaw",    "pitch", "health",  "on_ground",
                                                        "age", "width", "height", "pos",   "velocity"};

[[nodiscard]] inline std::string uuidOf(std::int32_t id) {
    char text[40];
    std::snprintf(text, sizeof text, "00000000-0000-4000-8000-%012d", id);
    return text;
}

[[nodiscard]] inline bool looksLikeUuid(std::string_view text) {
    if (text.size() != 36) {
        return false;
    }
    for (std::size_t at = 0; at < text.size(); ++at) {
        const bool dash = at == 8 || at == 13 || at == 18 || at == 23;
        if (dash ? text[at] != '-' : std::isxdigit(static_cast<unsigned char>(text[at])) == 0) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline std::string withNamespace(std::string_view id) {
    return id.find(':') == std::string_view::npos ? "minecraft:" + std::string{id} : std::string{id};
}

[[nodiscard]] inline Entity* entityById(Sim& state, std::int32_t id) {
    for (Entity& entity : state.entities) {
        if (entity.id == id && entity.alive) {
            return &entity;
        }
    }
    return nullptr;
}

[[nodiscard]] inline std::int32_t handleFor(Sim& state, const Entity& entity) {
    for (std::size_t at = 0; at < state.handles.size(); ++at) {
        if (state.handles[at] == entity.id) {
            return static_cast<std::int32_t>(at) + 1;
        }
    }
    state.handles.push_back(entity.id);
    return static_cast<std::int32_t>(state.handles.size());
}

/// kOk with `out` set; STALE for a handle whose entity is gone or released, INVALID for one never given.
[[nodiscard]] inline std::int32_t resolve(Sim& state, std::int32_t handle, Entity*& out) {
    if (handle <= 0 || static_cast<std::size_t>(handle) > state.handles.size()) {
        return kErrInvalid;
    }
    const std::int32_t id = state.handles[static_cast<std::size_t>(handle - 1)];
    Entity* found = id == 0 ? nullptr : entityById(state, id);
    if (found == nullptr) {
        return kErrStale;
    }
    out = found;
    return kOk;
}

[[nodiscard]] inline std::optional<double> numberOf(const Value* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::int64_t* whole = value->asInt()) {
        return static_cast<double>(*whole);
    }
    if (const double* real = value->asFloat()) {
        return *real;
    }
    return std::nullopt;
}

[[nodiscard]] inline Value point(const std::array<double, 3>& at) {
    return Value{Value::Map{{"x", Value{at[0]}}, {"y", Value{at[1]}}, {"z", Value{at[2]}}}};
}

[[nodiscard]] inline std::optional<Value> baseValue(const Entity& entity, std::string_view name) {
    if (name == "id") {
        return Value{static_cast<std::int64_t>(entity.id)};
    }
    if (name == "type") {
        return Value{entity.type};
    }
    if (name == "world") {
        return Value{std::string{kWorld}};
    }
    if (name == "uuid") {
        return Value{entity.uuid};
    }
    if (name == "alive") {
        return Value{entity.alive};
    }
    if (name == "x" || name == "y" || name == "z") {
        return Value{entity.pos[name == "x" ? 0 : name == "y" ? 1 : 2]};
    }
    if (name == "pos" || name == "velocity") {
        return point(name == "pos" ? entity.pos : entity.velocity);
    }
    if (name == "yaw") {
        return Value{entity.yaw};
    }
    if (name == "pitch") {
        return Value{entity.pitch};
    }
    if (name == "health") {
        return Value{entity.health};
    }
    if (name == "on_ground") {
        return Value{false};
    }
    if (name == "age") {
        return Value{std::int64_t{0}};
    }
    if (name == "width") {
        return Value{0.6};
    }
    if (name == "height") {
        return Value{1.8};
    }
    return std::nullopt;
}

struct Property {
    std::string_view name;
    bool writable;
    Value initial;
};

[[nodiscard]] inline const std::vector<Property>& properties() {
    static const std::vector<Property> kAll = {
        {"fire_ticks", true, Value{std::int64_t{0}}},
        {"glowing", true, Value{false}},
        {"custom_name", true, Value{}},
        {"tags", true, Value{Value::List{}}},
        {"vehicle", false, Value{std::int64_t{-1}}},
        {"no_ai", true, Value{false}},
    };
    return kAll;
}

[[nodiscard]] inline const Property* propertyNamed(std::string_view name) {
    for (const Property& property : properties()) {
        if (property.name == name) {
            return &property;
        }
    }
    return nullptr;
}

[[nodiscard]] inline Value* storedProperty(Entity& entity, const Property& property) {
    for (auto& [name, value] : entity.props) {
        if (name == property.name) {
            return &value;
        }
    }
    entity.props.emplace_back(std::string{property.name}, property.initial);
    return &entity.props.back().second;
}

[[nodiscard]] inline std::expected<Value, std::int32_t> optionsOf(const GuestMemory& memory, std::int32_t ptr,
                                                                  std::int32_t len) {
    if (len == 0) {
        return Value{Value::Map{}};
    }
    std::expected<Value, std::int32_t> value = readValue(memory, ptr, len);
    if (value && value->asMap() == nullptr) {
        return std::unexpected(kErrInvalid);
    }
    return value;
}

inline Rec& showOptions(Rec& rec, std::int32_t len, const std::expected<Value, std::int32_t>& value) {
    return len == 0 ? rec.raw("none") : rec.value(value);
}

inline std::int32_t doneValue(Rec& rec, const GuestMemory& memory, std::int32_t ptr, std::int32_t cap,
                              const Value& value) {
    const std::int32_t result = putValue(memory, ptr, cap, value);
    return result >= 0 && result <= cap ? rec.done(result, canonical(value)) : rec.done(result);
}

inline std::int32_t doneHandles(Rec& rec, const GuestMemory& memory, std::int32_t ptr, std::int32_t cap,
                                const std::vector<std::int32_t>& handles) {
    std::string list = "[";
    for (std::size_t at = 0; at < handles.size(); ++at) {
        list += (at == 0 ? "" : ", ") + std::to_string(handles[at]);
    }
    list += "]";
    const auto count = static_cast<std::int32_t>(handles.size());
    if (cap < 0 || count > cap) {
        return rec.done(cap < 0 ? kErrInvalid : count);
    }
    const auto view = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(count) * 4U);
    if (!view) {
        return rec.done(kErrInvalid);
    }
    if (count > 0) {
        std::memcpy(view->data(), handles.data(), view->size());
    }
    return rec.done(count, list);
}

} // namespace basalt::wire::wasm::trace::entitiesSim
