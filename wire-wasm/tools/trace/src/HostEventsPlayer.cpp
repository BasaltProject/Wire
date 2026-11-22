#include "HostEventsState.hpp"

#include <algorithm>
#include <cmath>

namespace basalt::wire::wasm::trace {
namespace {
enum class PlayerKind : std::uint8_t { Bool, Int, Real, Text, List };

struct PropertyRow {
    std::string_view name;
    PlayerKind kind;
    bool writable;
};

constexpr PropertyRow kProperties[] = {
#include "HostPlayerProps.inc"
};

constexpr std::string_view kOperations[] = {
#include "HostPlayerOps.inc"
};

[[nodiscard]] const PropertyRow* propertyRow(std::string_view name) {
    for (const PropertyRow& row : kProperties) {
        if (row.name == name) {
            return &row;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string_view kindName(PlayerKind kind) {
    constexpr std::string_view kNames[] = {"bool", "int", "real", "text", "list"};
    return kNames[static_cast<std::size_t>(kind)];
}

[[nodiscard]] Value defaultOf(PlayerKind kind) {
    switch (kind) {
    case PlayerKind::Bool:
        return Value{false};
    case PlayerKind::Int:
        return Value{std::int64_t{0}};
    case PlayerKind::Real:
        return Value{0.0};
    case PlayerKind::Text:
        return Value{std::string{}};
    case PlayerKind::List:
        return Value{Value::List{}};
    }
    return Value{};
}

[[nodiscard]] std::optional<double> numberOf(const Value& value) {
    if (const double* real = value.asFloat()) {
        return *real;
    }
    if (const std::int64_t* whole = value.asInt()) {
        return static_cast<double>(*whole);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Value> fittedProperty(PlayerKind kind, const Value& value) {
    switch (kind) {
    case PlayerKind::Bool:
        return value.asBool() != nullptr ? std::optional{value} : std::nullopt;
    case PlayerKind::Int:
        if (const double* real = value.asFloat(); real != nullptr && std::trunc(*real) == *real) {
            return Value{static_cast<std::int64_t>(*real)};
        }
        return value.asInt() != nullptr ? std::optional{value} : std::nullopt;
    case PlayerKind::Real:
        if (const std::optional<double> real = numberOf(value)) {
            return Value{*real};
        }
        return std::nullopt;
    case PlayerKind::Text:
        return value.asString() != nullptr ? std::optional{value} : std::nullopt;
    case PlayerKind::List:
        return value.asList() != nullptr ? std::optional{value} : std::nullopt;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Value> builtin(World& world, const Player& player, std::string_view name) {
    const PlayerState& state = playerStateOf(world, player);
    if (name == "name") {
        return Value{player.name};
    }
    if (name == "uuid") {
        return Value{uuidOf(player)};
    }
    if (name == "online") {
        return Value{player.online};
    }
    if (name == "health") {
        return Value{state.health};
    }
    if (name == "food") {
        return Value{state.food};
    }
    if (name == "gamemode") {
        return Value{state.gamemode};
    }
    if (name == "level") {
        return Value{state.level};
    }
    if (name == "world") {
        return Value{state.world};
    }
    if (name == "x") {
        return Value{state.x};
    }
    if (name == "y") {
        return Value{state.y};
    }
    if (name == "z") {
        return Value{state.z};
    }
    if (name == "yaw") {
        return Value{static_cast<double>(state.yaw)};
    }
    if (name == "pitch") {
        return Value{static_cast<double>(state.pitch)};
    }
    if (name == "ping") {
        return Value{std::int64_t{12}};
    }
    if (name == "sneaking" || name == "flying") {
        return Value{false};
    }
    if (name == "on_ground") {
        return Value{true};
    }
    if (name == "held_item") {
        return Value{};
    }
    if (name == "address") {
        return Value{std::string{"127.0.0.1:25565"}};
    }
    if (name == "permission_level") {
        return Value{std::int64_t{0}};
    }
    return std::nullopt;
}

void playerByUuid(void* u, Call& c) {
    World& world = worldOf(u);
    const auto uuid = c.memory().string(c.i32(0), c.i32(1));
    std::int32_t handle = 0;
    for (const Player& player : world.players) {
        if (uuid && player.online && uuidOf(player) == *uuid) {
            handle = player.handle;
        }
    }
    c.ret(Rec("player_by_uuid").str(uuid).done(handle));
}

void playerUuid(void* u, Call& c) {
    Player* player = nullptr;
    Rec rec("player_uuid");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = checkPlayer(worldOf(u), c.i32(0), player);
    c.ret(status != kOk ? rec.done(status) : rec.doneBuffer(c.memory(), c.i32(1), c.i32(2), uuidOf(*player)));
}

void playerGet(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    Rec rec("player_get");
    rec.i(c.i32(0)).str(name).cap(c.i32(4));
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk || !name) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    std::optional<Value> value = builtin(world, *player, *name);
    if (!value) {
        if (const PropertyRow* row = propertyRow(*name)) {
            PlayerState& state = playerStateOf(world, *player);
            const auto stored = state.properties.find(std::string{*name});
            if (stored != state.properties.end()) {
                const std::string text = stored->second;
                std::expected<Value, std::string> decoded =
                    decodeValue({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
                value = decoded ? *decoded : defaultOf(row->kind);
            } else {
                value = defaultOf(row->kind);
            }
        }
    }
    if (!value) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    c.ret(doneValue(rec, memory, c.i32(3), c.i32(4), *value));
}

void playerSet(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    const auto value = readValue(memory, c.i32(3), c.i32(4));
    Rec rec("player_set");
    rec.i(c.i32(0)).str(name).value(value);
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk || !name || !value) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    PlayerState& state = playerStateOf(world, *player);
    if (*name == "health") {
        const std::optional<double> health = numberOf(*value);
        if (!health || !std::isfinite(*health) || *health < 0.0) {
            c.ret(rec.done(kErrInvalid));
            return;
        }
        state.health = *health;
        c.ret(rec.done(kOk));
        return;
    }
    if (*name == "food" || *name == "level") {
        const std::int64_t* whole = value->asInt();
        if (whole == nullptr || *whole < 0 || *whole > 1000000) {
            c.ret(rec.done(kErrInvalid));
            return;
        }
        (*name == "food" ? state.food : state.level) = *whole;
        c.ret(rec.done(kOk));
        return;
    }
    if (*name == "gamemode") {
        if (value->asString() == nullptr) {
            c.ret(rec.done(kErrInvalid));
            return;
        }
        state.gamemode = *value->asString();
        c.ret(rec.done(kOk));
        return;
    }
    const PropertyRow* row = propertyRow(*name);
    const std::optional<Value> fitted = row != nullptr ? fittedProperty(row->kind, *value) : std::nullopt;
    if (row == nullptr || !row->writable || !fitted) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    std::string encoded;
    (void)encodeValue(*fitted, encoded);
    state.properties[std::string{*name}] = encoded;
    c.ret(rec.done(kOk));
}

void playerCall(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto operation = memory.string(c.i32(1), c.i32(2));
    Rec rec("player_call");
    rec.i(c.i32(0)).str(operation);
    Player* player = nullptr;
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    std::expected<Value, std::int32_t> arguments = Value{Value::List{}};
    if (c.i32(4) != 0) {
        arguments = readValue(memory, c.i32(3), c.i32(4));
    }
    rec.value(arguments).cap(c.i32(6));
    if (status == kOk && (!operation || !arguments || arguments->asList() == nullptr)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        for (const Value& item : *arguments->asList()) {
            if (item.asMap() != nullptr || item.asBytes() != nullptr) {
                status = kErrInvalid;
            }
        }
    }
    if (status == kOk &&
        std::find(std::begin(kOperations), std::end(kOperations), *operation) == std::end(kOperations)) {
        status = kErrAbsent;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    emitLine("world call " + player->name + " " + std::string{*operation} + " " + canonical(*arguments));
    c.ret(doneValue(rec, memory, c.i32(5), c.i32(6), Value{}));
}

void playerDescribe(void*, Call& c) {
    Rec rec("player_describe");
    rec.cap(c.i32(1));
    Value::List properties;
    for (const PropertyRow& row : kProperties) {
        properties.emplace_back(Value::Map{{"name", Value{std::string{row.name}}},
                                           {"kind", Value{std::string{kindName(row.kind)}}},
                                           {"writable", Value{row.writable}}});
    }
    Value::List operations;
    for (const std::string_view name : kOperations) {
        operations.emplace_back(std::string{name});
    }
    const Value described{
        Value::Map{{"properties", Value{std::move(properties)}}, {"operations", Value{std::move(operations)}}}};
    const std::int32_t result = putValue(c.memory(), c.i32(0), c.i32(1), described);
    c.ret(result >= 0 && result <= c.i32(1)
              ? rec.done(result, std::to_string(std::size(kProperties)) + " properties " +
                                     std::to_string(std::size(kOperations)) + " operations")
              : rec.done(result));
}

void playerHealth(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    Rec rec("player_health");
    rec.i(c.i32(0));
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const double health = playerStateOf(world, *player).health;
    c.ret(c.memory().write(c.i32(1), health) ? rec.done(kOk, "out=" + number(health)) : rec.done(kErrInvalid));
}

void playerSetHealth(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    Rec rec("player_set_health");
    rec.i(c.i32(0)).f64(c.f64(1));
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk) {
        if (std::isfinite(c.f64(1)) && c.f64(1) >= 0.0) {
            playerStateOf(world, *player).health = c.f64(1);
        } else {
            status = kErrInvalid;
        }
    }
    c.ret(rec.done(status));
}

void playerGamemode(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    Rec rec("player_gamemode");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    c.ret(status != kOk ? rec.done(status)
                        : rec.doneBuffer(c.memory(), c.i32(1), c.i32(2), playerStateOf(world, *player).gamemode));
}

void playerSetGamemode(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const auto mode = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("player_set_gamemode");
    rec.i(c.i32(0)).str(mode);
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    const bool known =
        mode && (*mode == "survival" || *mode == "creative" || *mode == "adventure" || *mode == "spectator");
    if (status == kOk) {
        if (known) {
            playerStateOf(world, *player).gamemode = std::string{*mode};
            emitLine("world gamemode " + player->name + " " + std::string{*mode});
        } else {
            status = kErrInvalid;
        }
    }
    c.ret(rec.done(status));
}

void playerTeleport(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const auto target = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("player_teleport");
    rec.i(c.i32(0)).str(target).f64(c.f64(3)).f64(c.f64(4)).f64(c.f64(5)).f32(c.f32(6)).f32(c.f32(7));
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!target || !std::isfinite(c.f64(3)) || !std::isfinite(c.f64(4)) || !std::isfinite(c.f64(5)) ||
                          !std::isfinite(c.f32(6)) || !std::isfinite(c.f32(7)))) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        PlayerState& state = playerStateOf(world, *player);
        const std::string where = target->empty() ? state.world : std::string{*target};
        if (where != "world" && where != "world_nether") {
            status = kErrAbsent;
        } else {
            state.world = where;
            state.x = c.f64(3);
            state.y = c.f64(4);
            state.z = c.f64(5);
            state.yaw = c.f32(6);
            state.pitch = c.f32(7);
            emitLine("world teleport " + player->name + " " + where + " " + number(state.x) + " " + number(state.y) +
                     " " + number(state.z));
        }
    }
    c.ret(rec.done(status));
}

void playerTitle(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const GuestMemory memory = c.memory();
    const auto title = memory.string(c.i32(1), c.i32(2));
    const auto subtitle = memory.string(c.i32(3), c.i32(4));
    const auto args = readArgs(memory, c.i32(8), c.i32(9));
    Rec rec("player_title");
    rec.i(c.i32(0)).str(title).str(subtitle).i(c.i32(5)).i(c.i32(6)).i(c.i32(7)).args(args);
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!title || !subtitle || !args || c.i32(5) < 0 || c.i32(6) < 0 || c.i32(7) < 0)) {
        status = kErrInvalid;
    }
    std::optional<std::string> main;
    std::optional<std::string> sub = std::string{};
    if (status == kOk) {
        main = render(world, *title, *args);
        if (!subtitle->empty()) {
            sub = render(world, *subtitle, *args);
        }
        status = main && sub ? kOk : kErrAbsent;
    }
    if (status == kOk) {
        emitLine("world title " + player->name + " " + quote(*main) + " " + quote(*sub) + " " +
                 std::to_string(c.i32(5)) + "/" + std::to_string(c.i32(6)) + "/" + std::to_string(c.i32(7)));
    }
    c.ret(rec.done(status));
}

void playerKick(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const GuestMemory memory = c.memory();
    const auto text = memory.string(c.i32(1), c.i32(2));
    const auto args = readArgs(memory, c.i32(3), c.i32(4));
    Rec rec("player_kick");
    rec.i(c.i32(0)).str(text).args(args);
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!text || !args)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        const std::optional<std::string> shown = render(world, *text, *args);
        if (shown) {
            emitLine("world kick " + player->name + " " + quote(*shown));
            player->online = false;
        } else {
            status = kErrAbsent;
        }
    }
    c.ret(rec.done(status));
}

void playerEffect(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const auto effect = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("player_effect");
    rec.i(c.i32(0)).str(effect).i(c.i32(3)).i(c.i32(4));
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!effect || effect->empty() || c.i32(3) < -1 || c.i32(4) < 0)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        const bool known = effect->starts_with("minecraft:");
        if (known) {
            emitLine("world effect " + player->name + " " + std::string{*effect} + " " + std::to_string(c.i32(3)) +
                     " " + std::to_string(c.i32(4)));
        }
        status = known ? kOk : kErrAbsent;
    }
    c.ret(rec.done(status));
}
} // namespace

void addEventsPlayerImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"player_by_uuid", playerByUuid, user},
                              {"player_uuid", playerUuid, user},
                              {"player_get", playerGet, user},
                              {"player_set", playerSet, user},
                              {"player_call", playerCall, user},
                              {"player_describe", playerDescribe, user},
                              {"player_health", playerHealth, user},
                              {"player_set_health", playerSetHealth, user},
                              {"player_gamemode", playerGamemode, user},
                              {"player_set_gamemode", playerSetGamemode, user},
                              {"player_teleport", playerTeleport, user},
                              {"player_title", playerTitle, user},
                              {"player_kick", playerKick, user},
                              {"player_effect", playerEffect, user},
                          });
}

} // namespace basalt::wire::wasm::trace
