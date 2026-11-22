#include "HostPackets.hpp"

namespace basalt::wire::wasm::trace {
namespace packets {
namespace {
[[nodiscard]] Value info(const Traffic& traffic) {
    Value::Map map;
    if (traffic.packet == nullptr) {
        map.push_back({"kind", Value{"channel"}});
        map.push_back({"channel", Value{traffic.channel}});
        map.push_back({"size", Value{static_cast<std::int64_t>(traffic.bytes.size())}});
        map.push_back({"player_name", Value{traffic.playerName}});
        map.push_back({"player_uuid", Value{"uuid-" + traffic.playerName}});
        return Value{std::move(map)};
    }
    map.push_back({"kind", Value{"packet"}});
    map.push_back({"name", Value{std::string{traffic.packet->name}}});
    map.push_back({"phase", Value{std::string{traffic.packet->phase}}});
    map.push_back({"id", Value{static_cast<std::int64_t>(traffic.packet->id)}});
    map.push_back({"serverbound", Value{traffic.packet->serverbound}});
    map.push_back({"connection", Value{std::int64_t{1}}});
    map.push_back({"protocol", Value{std::int64_t{777}}});
    map.push_back({"translated", Value{false}});
    map.push_back({"size", Value{static_cast<std::int64_t>(traffic.bytes.size())}});
    map.push_back({"player_name", Value{traffic.playerName}});
    map.push_back({"player_uuid", Value{"uuid-" + traffic.playerName}});
    map.push_back({"cancelled", Value{traffic.cancelled}});
    map.push_back({"observer", Value{traffic.observer}});
    return Value{std::move(map)};
}

void ctxInfo(void* u, Call& c) {
    World& world = worldOf(u);
    Traffic* traffic = nullptr;
    Rec rec("packet_ctx_info");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = activeOf(world, c.i32(0), traffic);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const Value value = info(*traffic);
    const std::int32_t result = putValue(c.memory(), c.i32(1), c.i32(2), value);
    c.ret(result >= 0 && result <= c.i32(2) ? rec.done(result, canonical(value)) : rec.done(result));
}

void ctxBody(void* u, Call& c) {
    World& world = worldOf(u);
    Traffic* traffic = nullptr;
    Rec rec("packet_ctx_body");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = activeOf(world, c.i32(0), traffic);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const std::string_view bytes{reinterpret_cast<const char*>(traffic->bytes.data()), traffic->bytes.size()};
    const std::int32_t result = c.memory().put(c.i32(1), c.i32(2), bytes);
    c.ret(result >= 0 && result <= c.i32(2) ? rec.done(result, canonical(blob(traffic->bytes))) : rec.done(result));
}

void ctxPlayer(void* u, Call& c) {
    World& world = worldOf(u);
    Traffic* traffic = nullptr;
    Rec rec("packet_ctx_player");
    rec.i(c.i32(0));
    const std::int32_t status = activeOf(world, c.i32(0), traffic);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const Player* player = world.playerByName(traffic->playerName);
    c.ret(rec.done(player == nullptr ? 0 : player->handle));
}

[[nodiscard]] std::int32_t decidable(World& world, std::int32_t ctx, Traffic*& traffic) {
    const std::int32_t status = activeOf(world, ctx, traffic);
    if (status != kOk) {
        return status;
    }
    if (traffic->packet == nullptr) {
        return kErrInvalid;
    }
    return traffic->observer ? kErrDenied : kOk;
}

void ctxCancel(void* u, Call& c) {
    Traffic* traffic = nullptr;
    const std::int32_t status = decidable(worldOf(u), c.i32(0), traffic);
    if (status == kOk) {
        traffic->cancelled = true;
    }
    c.ret(Rec("packet_ctx_cancel").i(c.i32(0)).done(status));
}

void ctxSetBody(void* u, Call& c) {
    Traffic* traffic = nullptr;
    const std::int32_t length = c.i32(2);
    const auto view = length < 0 || static_cast<std::size_t>(length) > kMaxBody
                          ? std::nullopt
                          : c.memory().range(static_cast<std::uint32_t>(c.i32(1)), static_cast<std::uint32_t>(length));
    Rec rec("packet_ctx_set_body");
    rec.i(c.i32(0)).value(view ? blob(std::vector<std::uint8_t>{view->begin(), view->end()}) : Value{});
    std::int32_t status = decidable(worldOf(u), c.i32(0), traffic);
    if (status == kOk && !view) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        traffic->replacement = std::vector<std::uint8_t>{view->begin(), view->end()};
    }
    c.ret(rec.done(status));
}

void relayOnConnection(void* u, Call& c, bool inbound) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    const std::int32_t length = c.i32(4);
    const auto view = length < 0 || static_cast<std::size_t>(length) > kMaxBody
                          ? std::nullopt
                          : memory.range(static_cast<std::uint32_t>(c.i32(3)), static_cast<std::uint32_t>(length));
    const std::vector<std::uint8_t> body =
        view ? std::vector<std::uint8_t>{view->begin(), view->end()} : std::vector<std::uint8_t>{};
    Rec rec(inbound ? "packet_ctx_receive" : "packet_ctx_send");
    rec.i(c.i32(0)).str(name).value(view ? blob(body) : Value{}).i(c.i32(5) != 0 ? 1 : 0);
    Traffic* traffic = nullptr;
    std::int32_t status = activeOf(world, c.i32(0), traffic);
    if (status == kOk && traffic->packet == nullptr) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const auto matches = name ? resolve(*name) : std::nullopt;
    const bool exact = matches && matches->size() == 1 && (*matches)[0]->name == *name;
    if (!exact || (*matches)[0]->serverbound != inbound || !view) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    c.ret(rec.done(1));
    emitLine("world " + std::string{inbound ? "connection-received " : "connection-sent "} + traffic->playerName + " " +
             std::string{*name} + " " + canonical(blob(body)));
    if (c.i32(5) != 0) {
        emitLine("world listeners-skipped " + std::string{*name} + " (re-entrant)");
    }
}

void ctxSend(void* u, Call& c) { relayOnConnection(u, c, false); }

void ctxReceive(void* u, Call& c) { relayOnConnection(u, c, true); }
} // namespace
} // namespace packets

void addPacketsExtraImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"packet_ctx_info", packets::ctxInfo, user},
                              {"packet_ctx_body", packets::ctxBody, user},
                              {"packet_ctx_player", packets::ctxPlayer, user},
                              {"packet_ctx_cancel", packets::ctxCancel, user},
                              {"packet_ctx_set_body", packets::ctxSetBody, user},
                              {"packet_ctx_send", packets::ctxSend, user},
                              {"packet_ctx_receive", packets::ctxReceive, user},
                          });
}
} // namespace basalt::wire::wasm::trace
