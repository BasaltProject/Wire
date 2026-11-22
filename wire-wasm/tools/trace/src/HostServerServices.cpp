#include "HostServerState.hpp"

#include <algorithm>

namespace basalt::wire::wasm::trace {
namespace {
using namespace server;

constexpr std::int32_t kDefaultTimeout = 100;

void serviceRegister(void* u, Call& c) {
    World& world = worldOf(u);
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("service_register");
    rec.str(name).i(c.i32(2));
    if (!name || name->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    State& state = stateOf(world);
    const bool exported =
        std::find(world.exportedServices.begin(), world.exportedServices.end(), *name) != world.exportedServices.end();
    if (!exported) {
        c.ret(rec.done(kErrDenied));
        return;
    }
    if (std::any_of(state.offers.begin(), state.offers.end(), [&](const Offer& o) { return o.name == *name; })) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::int32_t handle = state.nextOffer++;
    state.offers.push_back(Offer{std::string{*name}, c.i32(2), handle});
    c.ret(rec.done(handle));
}

void serviceReply(void* u, Call& c) {
    World& world = worldOf(u);
    Rec rec("service_reply");
    rec.i(c.i32(0));
    Context* context = nullptr;
    std::int32_t status = world.checkContext(c.i32(0), ContextKind::Completion, context);
    if (status == kOk && !(context->result && context->value && context->value->find("caller") != nullptr)) {
        status = kErrInvalid;
    }
    const std::expected<Value, std::int32_t> value =
        c.i32(2) == 0 ? std::expected<Value, std::int32_t>{Value{}} : readValue(c.memory(), c.i32(1), c.i32(2));
    rec.value(value);
    if (status == kOk && !value) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        std::string encoded;
        static_cast<void>(encodeValue(*value, encoded));
        context->result = std::move(encoded);
    }
    c.ret(rec.done(status));
}

void serviceRemove(void* u, Call& c) {
    State& state = stateOf(worldOf(u));
    const std::size_t before = state.offers.size();
    std::erase_if(state.offers, [&c](const Offer& offer) { return offer.handle == c.i32(0); });
    c.ret(Rec("service_remove").i(c.i32(0)).done(state.offers.size() < before ? kOk : kErrAbsent));
}

void serviceHas(void* u, Call& c) {
    State& state = stateOf(worldOf(u));
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("service_has");
    rec.str(name);
    c.ret(rec.done(
        name && std::any_of(state.offers.begin(), state.offers.end(), [&](const Offer& o) { return o.name == *name; })
            ? 1
            : 0));
}

void serviceList(void* u, Call& c) {
    State& state = stateOf(worldOf(u));
    Rec rec("service_list");
    rec.cap(c.i32(1));
    std::vector<std::string> names;
    for (const Offer& offer : state.offers) {
        names.push_back(offer.name);
    }
    std::sort(names.begin(), names.end());
    c.ret(doneValue(rec, c, 0, listOfText(names)));
}

void serviceCall(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(0), c.i32(1));
    Rec rec("service_call");
    rec.str(name);
    const std::expected<Value, std::int32_t> args =
        c.i32(3) == 0 ? std::expected<Value, std::int32_t>{Value{}} : readValue(memory, c.i32(2), c.i32(3));
    rec.value(args).i(c.i32(4)).i(c.i32(5));
    if (!name || name->empty() || !args) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    State& state = stateOf(world);
    const auto offer =
        std::find_if(state.offers.begin(), state.offers.end(), [&](const Offer& o) { return o.name == *name; });
    if (offer == state.offers.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const std::int32_t request = beginRequest(world, c.i32(5));
    const std::int32_t handlerToken = offer->token;
    const std::string service{*name};
    const std::int32_t timeout = c.i32(4) > 0 ? c.i32(4) : kDefaultTimeout;
    static_cast<void>(timeout);
    world.deferred.push_back([&world, request, handlerToken, service, value = *args] {
        if (!requestPending(world, request)) {
            return;
        }
        Context context;
        context.kind = ContextKind::Completion;
        context.value = mapOf({{"name", Value{service}}, {"caller", Value{"example"}}, {"args", value}});
        context.result = std::string{};
        const std::int32_t status = world.invoke(handlerToken, std::move(context));
        Value answer = mapOf({{"ok", Value{false}}, {"error", Value{"failed"}}});
        if (status == kOk && world.current.result && !world.current.result->empty()) {
            if (auto decoded = decodeValue(
                    std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t*>(world.current.result->data()),
                                                  world.current.result->size()})) {
                answer = mapOf({{"ok", Value{true}}, {"value", std::move(*decoded)}});
            }
        } else if (status == kOk) {
            answer = mapOf({{"ok", Value{true}}, {"value", Value{}}});
        }
        static_cast<void>(completeRequest(world, request, std::move(answer)));
    });
    c.ret(rec.done(request));
}

void taskWait(void* u, Call& c) {
    World& world = worldOf(u);
    Rec rec("task_wait");
    rec.i(c.i32(0)).i(c.i32(1));
    const std::int32_t request = beginRequest(world, c.i32(1));
    world.timers.emplace_back(world.now + std::max<std::int32_t>(c.i32(0), 1),
                              [&world, request] { static_cast<void>(completeRequest(world, request, Value{})); });
    c.ret(rec.done(request));
}

void taskAt(void* u, Call& c) {
    World& world = worldOf(u);
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("task_at");
    rec.str(name).f64(c.f64(2)).f64(c.f64(3)).i(c.i32(4));
    if (!name || name->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (*name != "world") {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const std::int32_t request = beginRequest(world, c.i32(4));
    world.deferred.push_back([&world, request] { static_cast<void>(completeRequest(world, request, Value{})); });
    c.ret(rec.done(request));
}
} // namespace

void addServerServicesImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"service_register", serviceRegister, user},
                              {"service_reply", serviceReply, user},
                              {"service_remove", serviceRemove, user},
                              {"service_has", serviceHas, user},
                              {"service_list", serviceList, user},
                              {"service_call", serviceCall, user},
                              {"task_wait", taskWait, user},
                              {"task_at", taskAt, user},
                          });
}

} // namespace basalt::wire::wasm::trace
