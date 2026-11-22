#pragma once

#include "HostApi.hpp"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::wasm::trace {

struct EventRow {
    std::string_view name;
    bool cancellable;
    std::string_view fields;
    std::string_view mutables;
};

/// Every event the server declares, generated from its definitions (gen-events.py), sorted by name.
[[nodiscard]] const std::vector<EventRow>& eventRows();
[[nodiscard]] const EventRow* eventRow(std::string_view name);
[[nodiscard]] std::vector<std::string> splitList(std::string_view text);

struct EventListener {
    std::string event;
    std::int32_t priority = 0;
    std::int32_t flags = 0;
    Value::Map filters;
    std::int32_t token = 0;
};

struct PlayerState {
    double health = 20.0;
    std::string gamemode = "survival";
    std::string world = "world";
    double x = 0.0;
    double y = 64.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    std::int64_t food = 20;
    std::int64_t level = 0;
    std::map<std::string, std::string> properties;
};

struct FormState {
    std::int32_t request = 0;
    Value form;
};

/// What the events area of the trace host keeps: listeners, the event being delivered, players, forms and dialogs.
struct EventsTrace {
    std::vector<EventListener> listeners;
    bool post = false;
    std::optional<std::string> cancelMessage;
    std::map<std::string, PlayerState> players;
    std::map<std::string, FormState> forms;
    std::map<std::string, std::int32_t> dialogs;
    std::int32_t emitDepth = 0;
};

[[nodiscard]] inline EventsTrace& eventsOf(World& world) { return world.areaState<EventsTrace>("events"); }

[[nodiscard]] inline PlayerState& playerStateOf(World& world, const Player& player) {
    return eventsOf(world).players[player.name];
}

[[nodiscard]] inline std::string uuidOf(const Player& player) {
    std::string digits = std::to_string(player.handle);
    return "00000000-0000-0000-0000-" + std::string(12 - digits.size(), '0') + digits;
}

[[nodiscard]] inline Value* field(Value& map, std::string_view key) {
    if (auto* entries = std::get_if<Value::Map>(&map.data)) {
        for (MapEntry& entry : *entries) {
            if (entry.key == key) {
                return &entry.value;
            }
        }
    }
    return nullptr;
}

/// Writes `value` with the needed-length convention and records the call; the value shows when it fit.
inline std::int32_t doneValue(Rec& rec, const GuestMemory& memory, std::int32_t ptr, std::int32_t cap,
                              const Value& value) {
    const std::int32_t result = putValue(memory, ptr, cap, value);
    return result >= 0 && result <= cap ? rec.done(result, canonical(value)) : rec.done(result);
}

} // namespace basalt::wire::wasm::trace
