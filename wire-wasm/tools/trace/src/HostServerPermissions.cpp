#include "HostServerState.hpp"
#include "Scenario.hpp"

#include <algorithm>

namespace basalt::wire::wasm::trace {
namespace {
using namespace server;

void permissionCheck(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto subject = memory.string(c.i32(0), c.i32(1));
    const auto node = memory.string(c.i32(2), c.i32(3));
    Rec rec("permission_check");
    rec.str(subject).str(node);
    if (!subject || !node || node->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const Scope scope{stateOf(world)};
    const std::int32_t answer = chain(world, *subject, *node);
    c.ret(rec.done(answer < 0 ? kErrAbsent : answer));
}

void permissionMeta(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto subject = memory.string(c.i32(0), c.i32(1));
    const auto key = memory.string(c.i32(2), c.i32(3));
    Rec rec("permission_meta");
    rec.str(subject).str(key).cap(c.i32(5));
    if (!subject || !key || key->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const Scope scope{stateOf(world)};
    const std::optional<std::string> found = chainMeta(world, *subject, *key);
    c.ret(found ? rec.doneBuffer(memory, c.i32(4), c.i32(5), *found) : rec.done(kErrAbsent));
}

void permissionHas(void* u, Call& c) {
    World& world = worldOf(u);
    const auto node = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("permission_has");
    rec.i(c.i32(0)).str(node);
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk || !node || node->empty()) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    const Scope scope{stateOf(world)};
    c.ret(rec.done(chain(world, player->name, *node) == 1 ? 1 : 0));
}

void permissionSet(void* u, Call& c) {
    World& world = worldOf(u);
    const auto node = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("permission_set");
    rec.i(c.i32(0)).str(node).i(c.i32(3));
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk || !node || node->empty() || c.i32(3) < -1 || c.i32(3) > 1) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    State& state = stateOf(world);
    if (c.i32(3) < 0) {
        state.grants[player->name].erase(std::string{*node});
    } else {
        state.grants[player->name][std::string{*node}] = c.i32(3) == 1;
    }
    c.ret(rec.done(kOk));
    emitLine("world permission " + player->name + " " + quote(*node) + " " +
             (c.i32(3) < 0 ? "dropped" : (c.i32(3) == 1 ? "granted" : "denied")));
    emitLine("world command-tree " + player->name);
}

void permissionProvide(void* u, Call& c) {
    State& state = stateOf(worldOf(u));
    Rec rec("permission_provide");
    rec.i(c.i32(0)).i(c.i32(1)).i(c.i32(2));
    if (c.i32(0) <= 0 || c.i32(1) < 1 || c.i32(1) > 3) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::int32_t handle = state.nextProvider++;
    state.providers.push_back(Provider{handle, c.i32(0), c.i32(1), c.i32(2)});
    c.ret(rec.done(handle));
}

void permissionProviderRemove(void* u, Call& c) {
    State& state = stateOf(worldOf(u));
    const std::size_t before = state.providers.size();
    std::erase_if(state.providers, [&c](const Provider& provider) { return provider.handle == c.i32(0); });
    c.ret(Rec("permission_provider_remove").i(c.i32(0)).done(state.providers.size() < before ? kOk : kErrAbsent));
}

[[nodiscard]] std::int32_t queryContext(World& world, std::int32_t id, std::string_view kind, Context*& context) {
    const std::int32_t status = world.checkContext(id, ContextKind::Completion, context);
    if (status != kOk) {
        return status;
    }
    return context->result && context->value && textOf(context->value->find("kind")) == kind ? kOk : kErrInvalid;
}

void permissionAnswer(void* u, Call& c) {
    Context* context = nullptr;
    std::int32_t status = queryContext(worldOf(u), c.i32(0), "check", context);
    if (status == kOk && (c.i32(1) < -1 || c.i32(1) > 1)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        context->result = c.i32(1) < 0 ? "-" : (c.i32(1) == 1 ? "1" : "0");
    }
    c.ret(Rec("permission_answer").i(c.i32(0)).i(c.i32(1)).done(status));
}

void permissionAnswerMeta(void* u, Call& c) {
    const auto text = c.memory().string(c.i32(1), c.i32(2));
    Context* context = nullptr;
    std::int32_t status = queryContext(worldOf(u), c.i32(0), "meta", context);
    if (status == kOk && !text) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        context->result = "=" + std::string{*text};
    }
    c.ret(Rec("permission_answer_meta").i(c.i32(0)).str(text).done(status));
}

void permissionChanged(void*, Call& c) {
    c.ret(Rec("permission_changed").done(kOk));
    emitLine("world command-trees resent");
}

[[nodiscard]] bool word(std::string_view& rest, std::string_view& out) {
    while (rest.starts_with(' ')) {
        rest.remove_prefix(1);
    }
    const std::size_t end = rest.find(' ');
    out = rest.substr(0, end);
    rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end);
    return !out.empty();
}
} // namespace

namespace {
bool askVerb(World& world, std::string_view rest) {
    std::string_view subject;
    std::string_view node;
    if (!word(rest, subject) || !word(rest, node)) {
        return false;
    }
    const std::int32_t answer = chain(world, subject, node);
    emitLine("world ask " + std::string{subject} + " " + quote(node) + " = " +
             (answer < 0 ? "undecided" : (answer == 1 ? "granted" : "denied")));
    return true;
}

bool askMetaVerb(World& world, std::string_view rest) {
    std::string_view subject;
    std::string_view key;
    if (!word(rest, subject) || !word(rest, key)) {
        return false;
    }
    const std::optional<std::string> found = chainMeta(world, subject, key);
    emitLine("world askmeta " + std::string{subject} + " " + quote(key) + " = " +
             (found ? quote(*found) : std::string{"none"}));
    return true;
}

const ScenarioVerbRegistrar kAskVerb{"ask", askVerb};
const ScenarioVerbRegistrar kAskMetaVerb{"askmeta", askMetaVerb};
} // namespace

void addServerPermissionsImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"permission_check", permissionCheck, user},
                              {"permission_meta", permissionMeta, user},
                              {"permission_has", permissionHas, user},
                              {"permission_set", permissionSet, user},
                              {"permission_provide", permissionProvide, user},
                              {"permission_provider_remove", permissionProviderRemove, user},
                              {"permission_answer", permissionAnswer, user},
                              {"permission_answer_meta", permissionAnswerMeta, user},
                              {"permission_changed", permissionChanged, user},
                          });
}

} // namespace basalt::wire::wasm::trace
