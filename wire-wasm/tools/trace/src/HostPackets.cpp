#include "HostPackets.hpp"

#include "Scenario.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace basalt::wire::wasm::trace {
namespace packets {
namespace {
constexpr std::array<std::string_view, 21> kActions{
    "use_item",     "use_item_on", "container_click", "interact",     "attack",           "custom_payload",
    "chat_command", "move",        "pick_block",      "pick_entity",  "place_recipe",     "creative_slot",
    "sign_update",  "rename_item", "beacon",          "select_trade", "container_button", "container_close",
    "held_slot",    "chat",        "edit_book"};

constexpr std::array<std::string_view, 5> kPriorities{"first", "early", "normal", "late", "last"};

[[nodiscard]] bool fits(const Packet& packet, std::string_view direction, std::string_view phase) {
    const bool wantsServerbound = direction == "serverbound";
    return packet.serverbound == wantsServerbound && (phase.empty() || packet.phase == phase);
}
} // namespace

Value blob(const std::vector<std::uint8_t>& bytes) { return Value{Blob{bytes}}; }

std::optional<std::vector<const Packet*>> resolve(std::string_view pattern) {
    std::vector<const Packet*> found;
    if (pattern == "*") {
        for (const Packet& packet : kPackets) {
            found.push_back(&packet);
        }
        return found;
    }
    for (const Packet& packet : kPackets) {
        if (packet.name == pattern) {
            return std::vector<const Packet*>{&packet};
        }
    }
    if (!pattern.ends_with(".*")) {
        return std::nullopt;
    }
    pattern.remove_suffix(2);
    const std::size_t dot = pattern.find('.');
    const std::string_view direction = pattern.substr(0, dot);
    const std::string_view phase = dot == std::string_view::npos ? std::string_view{} : pattern.substr(dot + 1);
    if (direction != "serverbound" && direction != "clientbound") {
        return std::nullopt;
    }
    for (const Packet& packet : kPackets) {
        if (fits(packet, direction, phase)) {
            found.push_back(&packet);
        }
    }
    return found.empty() ? std::nullopt : std::make_optional(found);
}

std::int32_t activeOf(World& world, std::int32_t ctx, Traffic*& out) {
    Context* context = nullptr;
    const std::int32_t status = world.checkContext(ctx, ContextKind::Event, context);
    if (status != kOk) {
        return status;
    }
    if (stateOf(world).active == nullptr) {
        return kErrInvalid;
    }
    out = stateOf(world).active;
    return kOk;
}

bool fire(World& world, Traffic& traffic) {
    State& state = stateOf(world);
    std::vector<std::size_t> order;
    for (const bool observers : {false, true}) {
        std::vector<std::size_t> group;
        for (std::size_t index = 0; index < state.listeners.size(); ++index) {
            const Listener& listener = state.listeners[index];
            const bool matches = std::ranges::find(listener.matches, traffic.packet) != listener.matches.end();
            if (listener.alive && listener.observer == observers && matches) {
                group.push_back(index);
            }
        }
        std::ranges::stable_sort(group, [&state](std::size_t a, std::size_t b) {
            return state.listeners[a].priority < state.listeners[b].priority;
        });
        order.insert(order.end(), group.begin(), group.end());
    }
    traffic.cancelled = false;
    traffic.replacement.reset();
    if (world.current.live && !order.empty()) {
        emitLine("world listeners-skipped " + std::string{traffic.packet->name} + " (re-entrant)");
        return true;
    }
    for (const std::size_t index : order) {
        const Listener listener = state.listeners[index];
        if (traffic.cancelled && !listener.observer) {
            continue;
        }
        const bool wasCancelled = traffic.cancelled;
        const auto kept = traffic.replacement;
        traffic.observer = listener.observer;
        state.active = &traffic;
        Context context;
        context.kind = ContextKind::Event;
        context.eventName = std::string{traffic.packet->name};
        static_cast<void>(world.invoke(listener.token, std::move(context)));
        state.active = nullptr;
        if (listener.observer) {
            traffic.cancelled = wasCancelled;
            traffic.replacement = kept;
        }
    }
    return !traffic.cancelled;
}

namespace {
[[nodiscard]] std::optional<std::vector<std::uint8_t>> bodyOf(const GuestMemory& memory, std::int32_t ptr,
                                                              std::int32_t len) {
    if (len < 0 || static_cast<std::size_t>(len) > kMaxBody) {
        return std::nullopt;
    }
    const auto view = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(len));
    if (!view) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>{view->begin(), view->end()};
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>> unhex(std::string_view text) {
    if (text == "-") {
        return std::vector<std::uint8_t>{};
    }
    if (text.size() % 2 != 0) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> out;
    for (std::size_t at = 0; at < text.size(); at += 2) {
        char* end = nullptr;
        const std::string pair{text.substr(at, 2)};
        const long value = std::strtol(pair.c_str(), &end, 16);
        if (end != pair.c_str() + 2) {
            return std::nullopt;
        }
        out.push_back(static_cast<std::uint8_t>(value));
    }
    return out;
}

[[nodiscard]] const Packet* find(std::string_view name) {
    const auto found = resolve(name);
    return found && found->size() == 1 && (*found)[0]->name == name ? (*found)[0] : nullptr;
}

void listenOnChannel(void* u, Call& c) {
    State& state = stateOf(worldOf(u));
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("channel_listen");
    rec.str(name).i(c.i32(2));
    if (!name || name->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    state.channels.emplace_back(std::string{*name}, c.i32(2));
    c.ret(rec.done(kOk));
}

void channelSend(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    const auto body = bodyOf(memory, c.i32(3), c.i32(4));
    Rec rec("channel_send");
    rec.i(c.i32(0)).str(name).value(body ? blob(*body) : Value{});
    Player* player = nullptr;
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!name || name->empty() || !body)) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    c.ret(rec.done(1));
    emitLine("world channel-out " + player->name + " " + quote(*name) + " " + canonical(blob(*body)));
}

void listen(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    const GuestMemory memory = c.memory();
    const auto pattern = memory.string(c.i32(0), c.i32(1));
    Rec rec("packet_listen");
    rec.str(pattern);
    std::expected<Value, std::int32_t> options = Value{};
    if (c.i32(3) > 0) {
        options = readValue(memory, c.i32(2), c.i32(3));
    }
    rec.value(options).i(c.i32(4));
    Listener listener;
    listener.token = c.i32(4);
    bool bad = !pattern || !options;
    if (!bad && !options->isNull()) {
        bad = options->asMap() == nullptr;
        if (!bad) {
            if (const Value* priority = options->find("priority")) {
                const std::string* text = priority->asString();
                const auto at = text == nullptr ? kPriorities.end() : std::ranges::find(kPriorities, *text);
                bad = at == kPriorities.end();
                listener.priority = static_cast<std::int32_t>(at - kPriorities.begin());
            }
            if (const Value* observe = options->find("observe")) {
                bad = bad || observe->asBool() == nullptr;
                listener.observer = observe->asBool() != nullptr && *observe->asBool();
            }
        }
    }
    const auto matches = bad ? std::nullopt : resolve(*pattern);
    if (!matches) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    listener.pattern = std::string{*pattern};
    listener.matches = *matches;
    state.listeners.push_back(std::move(listener));
    c.ret(rec.done(static_cast<std::int32_t>(state.listeners.size())));
}

void unlisten(void* u, Call& c) {
    State& state = stateOf(worldOf(u));
    const std::int32_t handle = c.i32(0);
    Rec rec("packet_unlisten");
    rec.i(handle);
    if (handle < 1) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (static_cast<std::size_t>(handle) > state.listeners.size() ||
        !state.listeners[static_cast<std::size_t>(handle - 1)].alive) {
        c.ret(rec.done(kErrStale));
        return;
    }
    state.listeners[static_cast<std::size_t>(handle - 1)].alive = false;
    c.ret(rec.done(kOk));
}

void relay(void* u, Call& c, bool inbound) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    const auto body = bodyOf(memory, c.i32(3), c.i32(4));
    const bool intercept = c.i32(5) != 0;
    Rec rec(inbound ? "packet_receive" : "packet_send");
    rec.i(c.i32(0)).str(name).value(body ? blob(*body) : Value{}).i(intercept ? 1 : 0);
    const Packet* packet = name ? find(*name) : nullptr;
    Player* player = nullptr;
    std::int32_t status = packet == nullptr || packet->serverbound != inbound || !body ? kErrInvalid : kOk;
    if (status == kOk) {
        status = checkPlayer(world, c.i32(0), player);
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    c.ret(rec.done(1));
    Traffic traffic;
    traffic.packet = packet;
    traffic.playerName = player->name;
    traffic.bytes = *body;
    const bool delivered = intercept ? fire(world, traffic) : true;
    const std::vector<std::uint8_t>& final = traffic.replacement ? *traffic.replacement : traffic.bytes;
    emitLine("world " + std::string{inbound ? "packet-handled " : "packet-sent "} + player->name + " " +
             std::string{packet->name} + " " + (delivered ? canonical(blob(final)) : "cancelled"));
}

void packetSend(void* u, Call& c) { relay(u, c, false); }

void packetReceive(void* u, Call& c) { relay(u, c, true); }

void lookup(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(0), c.i32(1));
    Rec rec("packet_lookup");
    rec.str(name).cap(c.i32(3));
    const auto matches = name ? resolve(*name) : std::nullopt;
    if (!matches) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const bool exact = matches->size() == 1 && (*matches)[0]->name == *name;
    Value::Map map;
    map.push_back({"name", Value{std::string{*name}}});
    map.push_back({"phase", exact ? Value{std::string{(*matches)[0]->phase}} : Value{}});
    map.push_back({"serverbound", exact ? Value{(*matches)[0]->serverbound} : Value{}});
    map.push_back({"id", exact ? Value{static_cast<std::int64_t>((*matches)[0]->id)} : Value{}});
    map.push_back({"wildcard", Value{!exact}});
    const Value value{std::move(map)};
    const std::int32_t result = putValue(memory, c.i32(2), c.i32(3), value);
    c.ret(result >= 0 && result <= c.i32(3) ? rec.done(result, canonical(value)) : rec.done(result));
    static_cast<void>(u);
}

void names(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto prefix = memory.string(c.i32(0), c.i32(1));
    Rec rec("packet_names");
    rec.str(prefix).cap(c.i32(3));
    if (!prefix) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    Value::List list;
    for (const Packet& packet : kPackets) {
        if (packet.name.starts_with(*prefix)) {
            list.emplace_back(std::string{packet.name});
        }
    }
    const Value value{std::move(list)};
    const std::int32_t result = putValue(memory, c.i32(2), c.i32(3), value);
    c.ret(result >= 0 && result <= c.i32(3) ? rec.done(result, canonical(value)) : rec.done(result));
    static_cast<void>(u);
}

void actions(void* u, Call& c) {
    Rec rec("packet_player_actions");
    rec.cap(c.i32(1));
    Value::List list;
    for (const std::string_view action : kActions) {
        list.emplace_back(std::string{action});
    }
    const Value value{std::move(list)};
    const std::int32_t result = putValue(c.memory(), c.i32(0), c.i32(1), value);
    c.ret(result >= 0 && result <= c.i32(1) ? rec.done(result, canonical(value)) : rec.done(result));
    static_cast<void>(u);
}

void playerReceive(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto action = memory.string(c.i32(1), c.i32(2));
    const auto args = readValue(memory, c.i32(3), c.i32(4));
    Rec rec("packet_player_receive");
    rec.i(c.i32(0)).str(action).value(args);
    const Value::List* list = args ? args->asList() : nullptr;
    bool known = false;
    for (const std::string_view each : kActions) {
        known = known || (action && each == *action);
    }
    bool scalars = list != nullptr;
    for (const Value& item : list != nullptr ? *list : Value::List{}) {
        scalars = scalars && item.asMap() == nullptr && item.asBytes() == nullptr;
    }
    Player* player = nullptr;
    std::int32_t status = known && scalars ? kOk : kErrInvalid;
    if (status == kOk) {
        status = checkPlayer(world, c.i32(0), player);
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    c.ret(rec.done(1));
    emitLine("world player-receive " + player->name + " " + std::string{*action} + " " + canonical(*args));
    if (*action == "custom_payload" && list->size() == 2 && (*list)[0].asString() && (*list)[1].asString()) {
        const std::string& channel = *(*list)[0].asString();
        const std::string& data = *(*list)[1].asString();
        for (const auto& [listened, token] : stateOf(world).channels) {
            if (listened != channel || world.current.live) {
                continue;
            }
            Traffic traffic;
            traffic.channel = channel;
            traffic.playerName = player->name;
            traffic.bytes.assign(data.begin(), data.end());
            stateOf(world).active = &traffic;
            Context context;
            context.kind = ContextKind::Event;
            context.eventName = channel;
            context.player = player->handle;
            static_cast<void>(world.invoke(token, std::move(context)));
            stateOf(world).active = nullptr;
        }
    }
}

[[nodiscard]] Player* playerFor(World& world, std::string_view name) {
    Player* player = world.playerByName(name);
    if (player == nullptr) {
        emitLine("world packet-rejected " + std::string{name} + " (not online)");
    }
    return player;
}

[[nodiscard]] std::string_view word(std::string_view& rest) {
    const std::size_t end = rest.find(' ');
    const std::string_view taken = rest.substr(0, end);
    rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
    return taken;
}

bool packetTraffic(World& world, std::string_view rest, bool inbound) {
    const std::string_view playerName = word(rest);
    const std::string_view name = word(rest);
    const auto body = unhex(word(rest));
    const Packet* packet = find(name);
    Player* player = playerFor(world, playerName);
    if (packet == nullptr || packet->serverbound != inbound || !body) {
        return false;
    }
    if (player == nullptr) {
        return true;
    }
    Traffic traffic;
    traffic.packet = packet;
    traffic.playerName = player->name;
    traffic.bytes = *body;
    const bool delivered = fire(world, traffic);
    const std::vector<std::uint8_t>& final = traffic.replacement ? *traffic.replacement : traffic.bytes;
    emitLine("world packet " + std::string{name} + (delivered ? " delivered " + canonical(blob(final)) : " cancelled"));
    return true;
}

bool packetIn(World& world, std::string_view rest) { return packetTraffic(world, rest, true); }

bool packetOut(World& world, std::string_view rest) { return packetTraffic(world, rest, false); }

bool channelIn(World& world, std::string_view rest) {
    const std::string_view playerName = word(rest);
    const std::string_view channel = word(rest);
    Player* player = playerFor(world, playerName);
    if (player == nullptr) {
        return true;
    }
    std::int32_t heard = 0;
    for (const auto& [listened, token] : stateOf(world).channels) {
        if (listened != channel) {
            continue;
        }
        ++heard;
        Traffic traffic;
        traffic.channel = std::string{channel};
        traffic.playerName = player->name;
        traffic.bytes.assign(rest.begin(), rest.end());
        stateOf(world).active = &traffic;
        Context context;
        context.kind = ContextKind::Event;
        context.eventName = std::string{channel};
        context.player = player->handle;
        static_cast<void>(world.invoke(token, std::move(context)));
        stateOf(world).active = nullptr;
    }
    emitLine("world channel " + std::string{channel} + " listeners=" + std::to_string(heard));
    return true;
}

const ScenarioVerbRegistrar kPacketIn{"packet_in", packetIn};
const ScenarioVerbRegistrar kPacketOut{"packet_out", packetOut};
const ScenarioVerbRegistrar kChannelIn{"channel_in", channelIn};
} // namespace
} // namespace packets

void addPacketsImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"channel_listen", packets::listenOnChannel, user},
                              {"channel_send", packets::channelSend, user},
                              {"packet_listen", packets::listen, user},
                              {"packet_unlisten", packets::unlisten, user},
                              {"packet_send", packets::packetSend, user},
                              {"packet_receive", packets::packetReceive, user},
                              {"packet_lookup", packets::lookup, user},
                              {"packet_names", packets::names, user},
                              {"packet_player_actions", packets::actions, user},
                              {"packet_player_receive", packets::playerReceive, user},
                          });
}
} // namespace basalt::wire::wasm::trace
