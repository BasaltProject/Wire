#pragma once

#include "HostApi.hpp"

#include <basalt/wire/common/json/Json.hpp>

#include <array>
#include <map>
#include <set>

namespace basalt::wire::wasm::trace::server {

struct Provider {
    std::int32_t handle = 0;
    std::int32_t priority = 0;
    std::int32_t kinds = 0;
    std::int32_t token = 0;
};

struct ExCommand {
    std::string name;
    std::string origin;
    plugins::CommandSpec spec;
    std::int32_t token = 0;
    std::int32_t completeToken = -1;
};

struct Offer {
    std::string name;
    std::int32_t token = 0;
    std::int32_t handle = 0;
};

struct Stopwatch {
    std::string id;
    std::int64_t startedTick = 0;
};

struct State {
    bool inside = false;
    std::int32_t nextProvider = 1;
    std::int32_t nextOffer = 1;
    std::map<std::string, std::map<std::string, bool>> grants;
    std::vector<Provider> providers;
    std::vector<ExCommand> commands;
    std::vector<Offer> offers;
    std::map<std::string, Value> store;
    std::map<std::string, std::string> files;
    std::map<std::string, std::string> packaged{{"data/readme.txt", "packaged readme"}};
    std::map<std::string, std::string> settings{
        {"motd", "A Basalt server"}, {"max_players", "20"}, {"gamemode", "survival"}, {"port", "25565"}};
    double tickRate = 20.0;
    bool frozen = false;
    std::vector<Stopwatch> stopwatches;
    std::vector<std::array<std::string, 3>> links{{"website", "", "https://basaltmc.org"}};
};

[[nodiscard]] inline State& stateOf(World& world) { return world.areaState<State>("server"); }

struct Scope {
    explicit Scope(State& state) : state_(state), previous_(state.inside) { state.inside = true; }

    ~Scope() { state_.inside = previous_; }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    State& state_;
    bool previous_;
};

inline std::int32_t doneValue(Rec& rec, Call& c, std::int32_t outIndex, const Value& value) {
    std::string encoded;
    if (!encodeValue(value, encoded)) {
        return rec.done(kErrInvalid);
    }
    const std::int32_t result = c.memory().put(c.i32(outIndex), c.i32(outIndex + 1), encoded);
    return result >= 0 && result <= c.i32(outIndex + 1) ? rec.done(result, canonical(value)) : rec.done(result);
}

[[nodiscard]] inline Value mapOf(std::vector<MapEntry> entries) { return Value{Value::Map{std::move(entries)}}; }

[[nodiscard]] inline Value listOfText(const std::vector<std::string>& items) {
    Value::List list;
    for (const std::string& item : items) {
        list.emplace_back(item);
    }
    return Value{std::move(list)};
}

[[nodiscard]] inline std::string_view textOf(const Value* value) {
    const std::string* text = value != nullptr ? value->asString() : nullptr;
    return text != nullptr ? std::string_view{*text} : std::string_view{};
}

[[nodiscard]] Value fromToml(const common::toml::TomlValue& value);
[[nodiscard]] Value fromJson(const common::JsonValue& value);
[[nodiscard]] std::optional<common::JsonValue> toJson(const Value& value);

/// The deterministic uuid the trace gives a player.
[[nodiscard]] std::string uuidOfPlayer(const Player& player);

/// 1 granted, 0 denied, -1 undecided; providers are asked only when no plugin callback is running.
[[nodiscard]] std::int32_t chain(World& world, std::string_view subject, std::string_view node);

[[nodiscard]] std::optional<std::string> chainMeta(World& world, std::string_view subject, std::string_view key);

/// The `commands` capability or the `files` folders, from plugin.toml.
[[nodiscard]] bool granted(const World& world, std::string_view capability);

} // namespace basalt::wire::wasm::trace::server
