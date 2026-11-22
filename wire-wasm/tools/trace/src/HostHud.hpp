#pragma once

#include "HostApi.hpp"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::wasm::trace::hud {

struct Team {
    std::string name;
    std::map<std::string, Value> options;
    std::vector<std::string> members;
};

struct Board {
    bool main = false;
    bool alive = true;
    std::vector<std::string> objectives;
    std::map<std::string, Value> objectiveSpecs;
    std::map<std::string, std::map<std::string, std::int32_t>> scores;
    std::map<std::string, std::string> display;
    std::vector<Team> teams;
};

struct Bar {
    std::string id;
    bool keyed = false;
    bool alive = true;
    bool owned = false;
    std::map<std::string, Value> fields;
    std::vector<std::string> players;
};

struct Hologram {
    bool alive = true;
    std::int32_t entity = 0;
    std::string text;
    double x = 0;
    double y = 0;
    double z = 0;
    std::optional<std::vector<std::string>> audience;
};

struct Npc {
    bool alive = true;
    std::int32_t entity = 0;
    std::int32_t token = 0;
    std::string uuid;
    std::string name;
    double x = 0;
    double y = 0;
    double z = 0;
    double health = 20.0;
    std::int64_t flags = 0;
    std::int64_t pose = 0;
    std::int64_t held = 0;
    std::map<std::string, double> attributes;
    std::map<std::int32_t, std::pair<std::string, std::int64_t>> items;
    std::optional<std::vector<std::string>> audience;
};

struct Claim {
    bool alive = true;
    std::string surface;
    std::int32_t priority = 0;
    std::int32_t token = 0;
    std::optional<Value> fixed;
};

struct State {
    std::vector<Board> boards;
    std::vector<Bar> bars;
    std::vector<Hologram> holograms;
    std::vector<Npc> npcs;
    std::vector<Claim> claims;
    std::map<std::string, std::int32_t> shown;
    std::map<std::string, std::map<std::string, Value>> playerProps;
    bool answering = false;
    std::optional<Value> answer;
    std::int32_t nextEntity = 100;
    std::int32_t nextBar = 1;
    std::int32_t mainHandle = 0;
};

inline constexpr std::int32_t kPlayerEntityBase = 1000;

[[nodiscard]] inline State& stateOf(World& world) { return world.areaState<State>("hud"); }

[[nodiscard]] inline std::expected<Value, std::int32_t> readSpec(const GuestMemory& memory, std::int32_t ptr,
                                                                 std::int32_t len) {
    if (len == 0) {
        return Value{Value::Map{}};
    }
    std::expected<Value, std::int32_t> spec = readValue(memory, ptr, len);
    if (spec && spec->asMap() == nullptr) {
        return std::unexpected(kErrInvalid);
    }
    return spec;
}

[[nodiscard]] inline std::int32_t takeString(const Value& spec, std::string_view key, std::optional<std::string>& out) {
    const Value* found = spec.find(key);
    if (found == nullptr) {
        return kOk;
    }
    const std::string* text = found->asString();
    if (text == nullptr) {
        return kErrInvalid;
    }
    out = *text;
    return kOk;
}

[[nodiscard]] inline std::int32_t takeBool(const Value& spec, std::string_view key, std::optional<bool>& out) {
    const Value* found = spec.find(key);
    if (found == nullptr) {
        return kOk;
    }
    const bool* flag = found->asBool();
    if (flag == nullptr) {
        return kErrInvalid;
    }
    out = *flag;
    return kOk;
}

[[nodiscard]] inline std::int32_t takeInt(const Value& spec, std::string_view key, std::optional<std::int64_t>& out) {
    const Value* found = spec.find(key);
    if (found == nullptr) {
        return kOk;
    }
    const std::int64_t* whole = found->asInt();
    if (whole == nullptr) {
        return kErrInvalid;
    }
    out = *whole;
    return kOk;
}

[[nodiscard]] inline std::int32_t takeNumber(const Value& spec, std::string_view key, std::optional<double>& out) {
    const Value* found = spec.find(key);
    if (found == nullptr) {
        return kOk;
    }
    if (const double* real = found->asFloat()) {
        out = *real;
    } else if (const std::int64_t* whole = found->asInt()) {
        out = static_cast<double>(*whole);
    } else {
        return kErrInvalid;
    }
    return kOk;
}

/// Renders MiniMessage or an '@key' with the plugin language; ABSENT when the key is missing.
[[nodiscard]] inline std::int32_t renderPlain(const World& world, std::string_view source, std::string& out) {
    const std::optional<std::string> rendered = render(world, source, {});
    if (!rendered) {
        return kErrAbsent;
    }
    out = *rendered;
    return kOk;
}

[[nodiscard]] inline std::int32_t takeText(const World& world, const Value& spec, std::string_view key,
                                           std::optional<std::string>& out) {
    std::optional<std::string> raw;
    if (const std::int32_t status = takeString(spec, key, raw); status != kOk || !raw) {
        return status;
    }
    std::string rendered;
    const std::int32_t status = renderPlain(world, *raw, rendered);
    if (status == kOk) {
        out = std::move(rendered);
    }
    return status;
}

[[nodiscard]] inline Value stringList(const std::vector<std::string>& names) {
    Value::List list;
    for (const std::string& name : names) {
        list.emplace_back(name);
    }
    return Value{std::move(list)};
}

/// The result of a buffer-returning import with the value printed on a fit, as the other value hosts do.
inline std::int32_t doneValue(Rec& rec, const GuestMemory& memory, std::int32_t ptr, std::int32_t cap,
                              const Value& value) {
    const std::int32_t result = putValue(memory, ptr, cap, value);
    return result >= 0 && result <= cap ? rec.done(result, canonical(value)) : rec.done(result);
}

[[nodiscard]] inline std::string uuidOf(std::string_view name) { return "uuid-" + std::string{name}; }

[[nodiscard]] inline std::optional<std::vector<std::string>> readAudience(World& world, const Value& list) {
    const Value::List* items = list.asList();
    if (items == nullptr) {
        return std::nullopt;
    }
    std::vector<std::string> uuids;
    for (const Value& item : *items) {
        const std::string* text = item.asString();
        if (text == nullptr) {
            return std::nullopt;
        }
        const Player* player = world.playerByName(*text);
        uuids.push_back(player != nullptr && player->online ? uuidOf(player->name) : *text);
    }
    return uuids;
}

} // namespace basalt::wire::wasm::trace::hud
