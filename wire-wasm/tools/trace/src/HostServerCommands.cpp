#include "HostServerState.hpp"
#include "Scenario.hpp"

#include <algorithm>

namespace basalt::wire::wasm::trace {
namespace {
using namespace server;

[[nodiscard]] std::string keyOf(const plugins::CommandSpec& spec) {
    std::string key;
    for (const std::string& literal : spec.literals) {
        key += key.empty() ? literal : " " + literal;
    }
    return key;
}

[[nodiscard]] bool taken(const World& world, const plugins::CommandSpec& spec) {
    return std::any_of(world.commands.begin(), world.commands.end(),
                       [&spec](const RegisteredCommand& held) { return held.spec.literals == spec.literals; });
}

void add(World& world, std::string origin, plugins::CommandSpec spec, std::int32_t token, std::int32_t completeToken) {
    world.commands.push_back(RegisteredCommand{spec, token});
    stateOf(world).commands.push_back(ExCommand{spec.name(), std::move(origin), std::move(spec), token, completeToken});
}

void commandRegisterEx(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto spec = memory.string(c.i32(0), c.i32(1));
    const auto permission = memory.string(c.i32(2), c.i32(3));
    Rec rec("command_register_ex");
    rec.str(spec).str(permission).i(c.i32(4)).i(c.i32(5));
    if (!spec || !permission) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    auto parsed = plugins::CommandSpec::parse(
        *spec, permission->empty() ? std::nullopt : std::optional<std::string>{std::string{*permission}});
    if (!parsed || taken(world, *parsed)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    add(world, {}, std::move(*parsed), c.i32(4), c.i32(5));
    c.ret(rec.done(kOk));
}

void commandAlias(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(0), c.i32(1));
    const auto alias = memory.string(c.i32(2), c.i32(3));
    Rec rec("command_alias");
    rec.str(name).str(alias);
    if (!name || !alias || alias->empty() || alias->find(' ') != std::string_view::npos) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    std::vector<ExCommand> sources;
    for (const ExCommand& entry : stateOf(world).commands) {
        if (entry.name == *name && entry.origin.empty()) {
            sources.push_back(entry);
        }
    }
    if (sources.empty()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    for (ExCommand& source : sources) {
        source.spec.literals.front() = std::string{*alias};
        if (taken(world, source.spec)) {
            c.ret(rec.done(kErrInvalid));
            return;
        }
    }
    for (ExCommand& source : sources) {
        add(world, std::string{*name}, std::move(source.spec), source.token, source.completeToken);
    }
    c.ret(rec.done(kOk));
}

void commandUnregister(void* u, Call& c) {
    World& world = worldOf(u);
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("command_unregister");
    rec.str(name);
    if (!name || name->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    State& state = stateOf(world);
    std::int32_t removed = 0;
    std::erase_if(state.commands, [&](const ExCommand& entry) {
        if (entry.name != *name && entry.origin != *name) {
            return false;
        }
        std::erase_if(world.commands, [&entry](const RegisteredCommand& held) {
            return held.spec.literals == entry.spec.literals && held.token == entry.token;
        });
        ++removed;
        return true;
    });
    c.ret(rec.done(removed == 0 ? kErrAbsent : removed));
}

void commandRun(void* u, Call& c) {
    World& world = worldOf(u);
    const auto line = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("command_run");
    rec.str(line).i(c.i32(2));
    if (!granted(world, "commands")) {
        c.ret(rec.done(kErrDenied));
        return;
    }
    Player* player = nullptr;
    const std::int32_t status = c.i32(2) == 0 ? kOk : checkPlayer(world, c.i32(2), player);
    if (status != kOk || !line || line->empty()) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    c.ret(rec.done(1));
    emitLine("world run " + (player == nullptr ? std::string{"console"} : player->name) + " " + quote(*line));
}

void commandCompleteInput(void* u, Call& c) {
    World& world = worldOf(u);
    Rec rec("command_complete_input");
    rec.i(c.i32(0)).cap(c.i32(2));
    Context* context = nullptr;
    std::int32_t status = world.checkContext(c.i32(0), ContextKind::Command, context);
    if (status == kOk && !(context->value && context->result)) {
        status = kErrInvalid;
    }
    c.ret(status != kOk ? rec.done(status)
                        : rec.doneBuffer(c.memory(), c.i32(1), c.i32(2), std::string{textOf(&*context->value)}));
}

void commandCompleteAdd(void* u, Call& c) {
    World& world = worldOf(u);
    const auto text = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("command_complete_add");
    rec.i(c.i32(0)).str(text);
    Context* context = nullptr;
    std::int32_t status = world.checkContext(c.i32(0), ContextKind::Command, context);
    if (status == kOk && !(context->value && context->result)) {
        status = kErrInvalid;
    }
    if (status == kOk && (!text || text->empty() || text->find('\n') != std::string_view::npos)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        context->result->append(context->result->empty() ? "" : "\n");
        context->result->append(*text);
    }
    c.ret(rec.done(status));
}

[[nodiscard]] const plugins::CommandValue* argumentOf(const Context& context, std::string_view name) {
    for (std::size_t index = 0; index < context.spec.arguments.size() && index < context.values.size(); ++index) {
        if (context.spec.arguments[index].name == name) {
            return &context.values[index];
        }
    }
    return nullptr;
}

void commandArgF64(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    Rec rec("command_arg_f64");
    rec.i(c.i32(0)).str(name);
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Command, context);
    if (status != kOk || !name) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    const plugins::CommandValue* value = argumentOf(*context, *name);
    if (value == nullptr || std::holds_alternative<std::monostate>(*value)) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    double number = 0.0;
    if (const auto* real = std::get_if<double>(value)) {
        number = *real;
    } else if (const auto* whole = std::get_if<std::int64_t>(value)) {
        number = static_cast<double>(*whole);
    } else {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    c.ret(memory.write(c.i32(3), number) ? rec.done(kOk, "out=" + trace::number(number)) : rec.done(kErrInvalid));
}

void commandArgBool(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    Rec rec("command_arg_bool");
    rec.i(c.i32(0)).str(name);
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Command, context);
    if (status != kOk || !name) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    const plugins::CommandValue* value = argumentOf(*context, *name);
    if (value == nullptr || std::holds_alternative<std::monostate>(*value)) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const auto* flag = std::get_if<bool>(value);
    if (flag == nullptr || !memory.write(c.i32(3), std::int32_t{*flag ? 1 : 0})) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    c.ret(rec.done(kOk, std::string{"out="} + (*flag ? "1" : "0")));
}

void commandSource(void* u, Call& c) {
    Rec rec("command_source");
    rec.i(c.i32(0)).cap(c.i32(2));
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Command, context);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    c.ret(doneValue(rec, c, 1,
                    mapOf({{"name", Value{context->source}},
                           {"console", Value{context->console}},
                           {"kind", Value{context->console ? "console" : "player"}},
                           {"world", Value{context->console ? "" : "world"}},
                           {"x", Value{0.0}},
                           {"y", Value{0.0}},
                           {"z", Value{0.0}},
                           {"caller", Value{""}}})));
}

void commandSourcePlayer(void* u, Call& c) {
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Command, context);
    c.ret(Rec("command_source_player").i(c.i32(0)).done(status != kOk ? status : context->player));
}

[[nodiscard]] std::vector<std::string> splitWords(std::string_view line) {
    std::vector<std::string> words;
    std::size_t at = 0;
    while (at < line.size()) {
        while (at < line.size() && line[at] == ' ') {
            ++at;
        }
        const std::size_t start = at;
        while (at < line.size() && line[at] != ' ') {
            ++at;
        }
        if (at > start) {
            words.emplace_back(line.substr(start, at - start));
        }
    }
    return words;
}

void complete(World& world, std::string_view source, std::string_view line) {
    const std::vector<std::string> words = splitWords(line);
    const ExCommand* best = nullptr;
    for (const ExCommand& entry : stateOf(world).commands) {
        const auto& literals = entry.spec.literals;
        if (words.size() >= literals.size() && std::equal(literals.begin(), literals.end(), words.begin()) &&
            (best == nullptr || literals.size() > best->spec.literals.size())) {
            best = &entry;
        }
    }
    if (best == nullptr || best->completeToken < 0) {
        emitLine("world complete " + quote(line) + " = none");
        return;
    }
    const bool console = source == "console";
    Context context;
    context.kind = ContextKind::Command;
    context.spec = best->spec;
    context.source = console ? "CONSOLE" : std::string{source};
    context.console = console;
    if (!console) {
        if (const Player* found = world.playerByName(source)) {
            context.player = found->handle;
        }
    }
    context.value = Value{std::string{words.size() > best->spec.literals.size() ? words.back() : std::string{}}};
    context.result = std::string{};
    static_cast<void>(world.invoke(best->completeToken, std::move(context)));
    const std::string joined = world.current.result.value_or("");
    std::vector<std::string> candidates;
    for (std::size_t at = 0; at < joined.size();) {
        const std::size_t end = joined.find('\n', at);
        candidates.push_back(joined.substr(at, end == std::string::npos ? std::string::npos : end - at));
        at = end == std::string::npos ? joined.size() : end + 1;
    }
    std::string shown = "[";
    for (const std::string& candidate : candidates) {
        shown += (shown.size() > 1 ? ", " : "") + quote(candidate);
    }
    emitLine("world complete " + quote(line) + " = " + shown + "]");
}
} // namespace

namespace {
bool completeVerb(World& world, std::string_view rest) {
    while (rest.starts_with(' ')) {
        rest.remove_prefix(1);
    }
    const std::size_t end = rest.find(' ');
    if (end == std::string_view::npos) {
        return false;
    }
    complete(world, rest.substr(0, end), rest.substr(end + 1));
    return true;
}

const ScenarioVerbRegistrar kCompleteVerb{"complete", completeVerb};
} // namespace

void addServerCommandsImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"command_register_ex", commandRegisterEx, user},
                              {"command_complete_input", commandCompleteInput, user},
                              {"command_complete_add", commandCompleteAdd, user},
                              {"command_alias", commandAlias, user},
                              {"command_unregister", commandUnregister, user},
                              {"command_run", commandRun, user},
                              {"command_arg_f64", commandArgF64, user},
                              {"command_arg_bool", commandArgBool, user},
                              {"command_source", commandSource, user},
                              {"command_source_player", commandSourcePlayer, user},
                          });
}

} // namespace basalt::wire::wasm::trace
