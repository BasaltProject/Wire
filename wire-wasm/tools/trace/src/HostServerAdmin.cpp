#include "HostServerState.hpp"

#include <algorithm>

namespace basalt::wire::wasm::trace {
namespace {
using namespace server;

constexpr std::array<std::string_view, 37> kOps{"access.is_banned",
                                                "access.is_operator",
                                                "access.is_whitelisted",
                                                "access.known",
                                                "access.last_death",
                                                "access.set_statistic",
                                                "access.statistic",
                                                "datapacks.function_tags",
                                                "datapacks.functions",
                                                "datapacks.list",
                                                "datapacks.refresh",
                                                "datapacks.resource",
                                                "datapacks.resource_pack",
                                                "datapacks.resources",
                                                "datapacks.schedule",
                                                "datapacks.scheduled",
                                                "datapacks.unschedule",
                                                "links.list",
                                                "links.set",
                                                "settings.get",
                                                "settings.keys",
                                                "settings.set",
                                                "stopwatches.create",
                                                "stopwatches.elapsed",
                                                "stopwatches.list",
                                                "stopwatches.remove",
                                                "stopwatches.restart",
                                                "tick.frozen",
                                                "tick.pace",
                                                "tick.paused",
                                                "tick.rate",
                                                "tick.set_frozen",
                                                "tick.set_rate",
                                                "tick.sprint",
                                                "tick.step",
                                                "tick.stop_sprint",
                                                "tick.times"};

struct Run {
    World& world;
    State& state;
    const Value& args;
    std::int32_t status = kOk;

    [[nodiscard]] std::string_view text(std::string_view key) const { return textOf(args.find(key)); }

    Value invalid() {
        status = kErrInvalid;
        return Value{};
    }
};

[[nodiscard]] std::optional<std::int64_t> integerOf(const Value* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::int64_t* whole = value->asInt()) {
        return *whole;
    }
    return std::nullopt;
}

[[nodiscard]] Value known(std::string_view who) {
    if (who == "Alex" || who == "00000000-0000-0000-0000-000000000001") {
        return mapOf({{"uuid", Value{"00000000-0000-0000-0000-000000000001"}},
                      {"name", Value{"Alex"}},
                      {"first_seen", Value{std::int64_t{1700000000000}}},
                      {"last_seen", Value{std::int64_t{1700000100000}}}});
    }
    return Value{};
}

Value operate(Run& run, std::string_view op) {
    State& state = run.state;
    if (op == "settings.keys") {
        std::vector<std::string> keys;
        for (const auto& [key, value] : state.settings) {
            keys.push_back(key);
        }
        return listOfText(keys);
    }
    if (op == "settings.get") {
        const auto found = state.settings.find(std::string{run.text("key")});
        return found == state.settings.end() ? Value{} : Value{found->second};
    }
    if (op == "settings.set") {
        const Value* value = run.args.find("value");
        if (run.text("key").empty() || value == nullptr) {
            return run.invalid();
        }
        const auto found = state.settings.find(std::string{run.text("key")});
        if (found == state.settings.end() || found->first == "port") {
            return Value{false};
        }
        found->second =
            value->asString() != nullptr ? *value->asString() : std::to_string(integerOf(value).value_or(0));
        return Value{true};
    }
    if (op == "tick.rate") {
        return Value{state.tickRate};
    }
    if (op == "tick.set_rate") {
        const Value* rate = run.args.find("rate");
        const double* real = rate != nullptr ? rate->asFloat() : nullptr;
        if (real == nullptr || *real <= 0.0) {
            return run.invalid();
        }
        state.tickRate = *real;
        return Value{};
    }
    if (op == "tick.frozen") {
        return Value{state.frozen};
    }
    if (op == "tick.set_frozen") {
        const Value* flag = run.args.find("frozen");
        if (flag == nullptr || flag->asBool() == nullptr) {
            return run.invalid();
        }
        state.frozen = *flag->asBool();
        return Value{};
    }
    if (op == "tick.step") {
        const std::optional<std::int64_t> count = integerOf(run.args.find("count"));
        return count && *count > 0 ? Value{state.frozen} : run.invalid();
    }
    if (op == "tick.sprint") {
        const std::optional<std::int64_t> count = integerOf(run.args.find("count"));
        return count && *count > 0 ? Value{true} : run.invalid();
    }
    if (op == "tick.stop_sprint") {
        return Value{false};
    }
    if (op == "tick.pace") {
        return mapOf(
            {{"tps_1m", Value{20.0}}, {"tps_5m", Value{20.0}}, {"tps_15m", Value{20.0}}, {"mean_tick_ms", Value{1.5}}});
    }
    if (op == "tick.times") {
        return Value{Value::List{Value{1200000}, Value{1300000}}};
    }
    if (op == "tick.paused") {
        return Value{false};
    }
    if (op == "links.list") {
        Value::List list;
        for (const auto& link : state.links) {
            list.push_back(mapOf({{"kind", Value{link[0]}}, {"label", Value{link[1]}}, {"url", Value{link[2]}}}));
        }
        return Value{std::move(list)};
    }
    if (op == "links.set") {
        const Value* given = run.args.find("links");
        const Value::List* list = given != nullptr ? given->asList() : nullptr;
        if (list == nullptr) {
            return run.invalid();
        }
        state.links.clear();
        for (const Value& row : *list) {
            state.links.push_back({std::string{textOf(row.find("kind"))}, std::string{textOf(row.find("label"))},
                                   std::string{textOf(row.find("url"))}});
        }
        return Value{};
    }
    if (op == "access.is_banned") {
        return Value{run.text("who") == "Griefer"};
    }
    if (op == "access.is_whitelisted" || op == "access.is_operator") {
        return Value{run.text("who") == "Alex"};
    }
    if (op == "access.known") {
        return run.text("who").empty() ? run.invalid() : known(run.text("who"));
    }
    if (op == "access.statistic") {
        return run.text("uuid").empty() || run.text("key").empty() ? run.invalid() : Value{42};
    }
    if (op == "access.set_statistic") {
        return run.text("uuid").empty() || !integerOf(run.args.find("value")) ? run.invalid() : Value{true};
    }
    if (op == "access.last_death") {
        return mapOf(
            {{"dimension", Value{"minecraft:overworld"}}, {"x", Value{10}}, {"y", Value{64}}, {"z", Value{-5}}});
    }
    if (op == "stopwatches.list") {
        std::vector<std::string> ids;
        for (const Stopwatch& watch : state.stopwatches) {
            ids.push_back(watch.id);
        }
        return listOfText(ids);
    }
    const auto watch = std::find_if(state.stopwatches.begin(), state.stopwatches.end(),
                                    [&run](const Stopwatch& held) { return held.id == run.text("id"); });
    if (op.starts_with("stopwatches.") && run.text("id").empty()) {
        return run.invalid();
    }
    if (op == "stopwatches.create") {
        if (watch != state.stopwatches.end()) {
            return Value{false};
        }
        state.stopwatches.push_back(Stopwatch{std::string{run.text("id")}, run.world.now});
        return Value{true};
    }
    if (op == "stopwatches.restart") {
        if (watch == state.stopwatches.end()) {
            return Value{false};
        }
        watch->startedTick = run.world.now;
        return Value{true};
    }
    if (op == "stopwatches.remove") {
        if (watch == state.stopwatches.end()) {
            return Value{false};
        }
        state.stopwatches.erase(watch);
        return Value{true};
    }
    if (op == "stopwatches.elapsed") {
        return watch == state.stopwatches.end() ? Value{}
                                                : Value{static_cast<double>(run.world.now - watch->startedTick) / 20.0};
    }
    if (op == "datapacks.list") {
        return Value{Value::List{mapOf({{"id", Value{"vanilla"}},
                                        {"enabled", Value{true}},
                                        {"required", Value{true}},
                                        {"compatibility", Value{"compatible"}},
                                        {"source", Value{"built_in"}}})}};
    }
    if (op == "datapacks.refresh") {
        return Value{};
    }
    if (op == "datapacks.resource") {
        return run.text("name") == "data/minecraft/recipe/stick.json" ? Value{Blob{{0x7b, 0x7d}}} : Value{};
    }
    if (op == "datapacks.resources") {
        return listOfText({"data/minecraft/recipe/stick.json"});
    }
    if (op == "datapacks.resource_pack") {
        return run.text("name") == "data/minecraft/recipe/stick.json" ? Value{"vanilla"} : Value{};
    }
    if (op == "datapacks.functions" || op == "datapacks.function_tags") {
        return listOfText({});
    }
    if (op == "datapacks.scheduled") {
        return Value{Value::List{}};
    }
    if (op == "datapacks.schedule") {
        return run.text("function").empty() || !integerOf(run.args.find("delay")) ? run.invalid() : Value{false};
    }
    if (op == "datapacks.unschedule") {
        return run.text("name").empty() ? run.invalid() : Value{0};
    }
    return run.invalid();
}

void serverCall(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto op = memory.string(c.i32(0), c.i32(1));
    Rec rec("server_call");
    rec.str(op);
    std::expected<Value, std::int32_t> args = Value{};
    if (c.i32(3) != 0) {
        args = readValue(memory, c.i32(2), c.i32(3));
    }
    rec.value(args).cap(c.i32(5));
    if (!op || !args) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    State& state = stateOf(world);
    const Scope scope{state};
    if (*op == "ops") {
        std::vector<std::string> names;
        for (const std::string_view name : kOps) {
            names.emplace_back(name);
        }
        std::sort(names.begin(), names.end());
        names.emplace_back("ops");
        c.ret(doneValue(rec, c, 4, listOfText(names)));
        return;
    }
    const bool known = std::find(kOps.begin(), kOps.end(), *op) != kOps.end();
    if (!known) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (op->starts_with("datapacks.") && !granted(world, "datapacks")) {
        c.ret(rec.done(kErrDenied));
        return;
    }
    Run run{world, state, *args};
    const Value result = operate(run, *op);
    c.ret(run.status != kOk ? rec.done(run.status) : doneValue(rec, c, 4, result));
}
} // namespace

void addServerAdminImports(World& world, std::vector<HostImport>& out) {
    out.insert(out.end(), {{"server_call", serverCall, &world}});
}

} // namespace basalt::wire::wasm::trace
