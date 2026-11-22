#include "HostServerState.hpp"

namespace basalt::wire::wasm::trace {
namespace {
using namespace server;

void serverBroadcast(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto text = memory.string(c.i32(0), c.i32(1));
    const auto args = readArgs(memory, c.i32(2), c.i32(3));
    const std::int32_t flags = c.i32(4);
    Rec rec("server_broadcast");
    rec.str(text).args(args).i(flags);
    if (!text || !args || (flags & 3) == 0) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::optional<std::string> rendered = render(world, *text, *args);
    if (!rendered) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    std::vector<std::string> lines;
    std::int32_t reached = 0;
    if ((flags & 1) != 0) {
        for (const Player& player : world.players) {
            if (player.online) {
                lines.push_back("world broadcast " + player.name + " " + quote(*rendered));
                ++reached;
            }
        }
    }
    if ((flags & 2) != 0) {
        lines.push_back("world console " + quote(*rendered));
    }
    c.ret(rec.done(reached));
    for (const std::string& line : lines) {
        emitLine(line);
    }
}

void placeholderFill(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto text = memory.string(c.i32(0), c.i32(1));
    Rec rec("placeholder_fill");
    rec.str(text).i(c.i32(2)).cap(c.i32(4));
    Player* player = nullptr;
    const std::int32_t status = c.i32(2) == 0 ? kOk : checkPlayer(world, c.i32(2), player);
    if (status != kOk || !text) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    std::string filled{*text};
    if (player != nullptr) {
        for (std::size_t at = filled.find("%player_name%"); at != std::string::npos;
             at = filled.find("%player_name%")) {
            filled.replace(at, 13, player->name);
        }
    }
    c.ret(rec.doneBuffer(memory, c.i32(3), c.i32(4), filled));
}

void serverInfo(void* u, Call& c) {
    World& world = worldOf(u);
    Rec rec("server_info");
    rec.cap(c.i32(1));
    State& state = stateOf(world);
    std::int64_t online = 0;
    for (const Player& player : world.players) {
        online += player.online ? 1 : 0;
    }
    c.ret(doneValue(rec, c, 0,
                    mapOf({{"software", Value{"Basalt"}},
                           {"version", Value{"trace"}},
                           {"minecraft", Value{"26.3"}},
                           {"protocol", Value{777}},
                           {"tps_1m", Value{20.0}},
                           {"tps_5m", Value{20.0}},
                           {"tps_15m", Value{20.0}},
                           {"mean_tick_ms", Value{1.5}},
                           {"tick_rate", Value{state.tickRate}},
                           {"frozen", Value{state.frozen}},
                           {"paused", Value{false}},
                           {"online", Value{online}},
                           {"max_players", Value{20}},
                           {"plugins", Value{Value::List{mapOf({{"id", Value{"example"}},
                                                                {"name", Value{"Example"}},
                                                                {"version", Value{"1.0.0"}},
                                                                {"disabled", Value{false}}})}}},
                           {"plugin", mapOf({{"id", Value{"example"}},
                                             {"name", Value{"Example"}},
                                             {"version", Value{"1.0.0"}},
                                             {"authors", Value{Value::List{Value{"Whiron"}}}},
                                             {"lane", Value{0}},
                                             {"lanes", Value{1}},
                                             {"folder", Value{"plugins/example"}}})}})));
}

void profileLookup(void* u, Call& c) {
    World& world = worldOf(u);
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("profile_lookup");
    rec.str(name).i(c.i32(2));
    if (!name || name->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::int32_t request = beginRequest(world, c.i32(2));
    Value found;
    if (*name == "Notch") {
        found = mapOf({{"uuid", Value{"069a79f4-44e9-4726-a5be-fca90e38aaf5"}},
                       {"name", Value{"Notch"}},
                       {"texture", Value{"dGV4dHVyZXM="}},
                       {"signature", Value{"c2ln"}}});
    }
    world.deferred.push_back([&world, request, found = std::move(found)] {
        emitLine("world profile-lookup finished");
        static_cast<void>(completeRequest(world, request, found));
    });
    c.ret(rec.done(request));
}
} // namespace

void addServerInfoImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"server_broadcast", serverBroadcast, user},
                              {"placeholder_fill", placeholderFill, user},
                              {"server_info", serverInfo, user},
                              {"profile_lookup", profileLookup, user},
                          });
}

} // namespace basalt::wire::wasm::trace
