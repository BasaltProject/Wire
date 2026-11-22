#include "HostEntitiesState.hpp"

#include <cmath>
#include <map>

namespace basalt::wire::wasm::trace {
namespace {
using namespace entitiesSim;

constexpr std::array<std::string_view, 3> kCategories{"monster", "creature", "ambient"};

struct Rules {
    std::map<std::string, std::int64_t> limits;
    std::map<std::string, std::int64_t> ticksPer;
    bool allowMonsters = true;
    bool allowAnimals = true;
};

[[nodiscard]] bool isBase(std::string_view name) { return std::ranges::find(kBase, name) != kBase.end(); }

void entityGet(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const auto name = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("entity_get");
    rec.i(c.i32(0)).str(name).cap(c.i32(4));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    if (status != kOk || !name) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    if (const std::optional<Value> base = baseValue(*entity, *name)) {
        c.ret(doneValue(rec, c.memory(), c.i32(3), c.i32(4), *base));
        return;
    }
    const Property* property = entity->player ? nullptr : propertyNamed(*name);
    if (property == nullptr) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    c.ret(doneValue(rec, c.memory(), c.i32(3), c.i32(4), *storedProperty(*entity, *property)));
}

void entitySet(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    const std::expected<Value, std::int32_t> value = readValue(memory, c.i32(3), c.i32(4));
    Rec rec("entity_set");
    rec.i(c.i32(0)).str(name).value(value);
    Entity* entity = nullptr;
    std::int32_t status = resolve(state, c.i32(0), entity);
    if (status == kOk && (!name || !value)) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    if (*name == "velocity" || *name == "yaw" || *name == "pitch") {
        const std::optional<double> angle = numberOf(&*value);
        if (*name == "velocity") {
            const std::optional<double> x = numberOf(value->find("x"));
            const std::optional<double> y = numberOf(value->find("y"));
            const std::optional<double> z = numberOf(value->find("z"));
            if (!x || !y || !z || entity->player) {
                c.ret(rec.done(kErrInvalid));
                return;
            }
            entity->velocity = {*x, *y, *z};
        } else if (!angle) {
            c.ret(rec.done(kErrInvalid));
            return;
        } else {
            (*name == "yaw" ? entity->yaw : entity->pitch) = *angle;
        }
        c.ret(rec.done(kOk));
        return;
    }
    if (const Property* property = entity->player ? nullptr : propertyNamed(*name)) {
        if (!property->writable) {
            c.ret(rec.done(kErrInvalid));
            return;
        }
        *storedProperty(*entity, *property) = *value;
        c.ret(rec.done(kOk));
        return;
    }
    c.ret(rec.done(isBase(*name) ? kErrInvalid : kErrAbsent));
}

[[nodiscard]] Value rowsOfGoals(const Entity& entity) {
    Value::List rows;
    for (const Goal& goal : entity.goals) {
        rows.emplace_back(Value::Map{{"key", Value{goal.key}},
                                     {"priority", Value{goal.priority}},
                                     {"running", Value{false}},
                                     {"plugin", Value{true}},
                                     {"usable", Value{true}},
                                     {"flags", Value{goal.flags}}});
    }
    return Value{std::move(rows)};
}

[[nodiscard]] std::optional<Value> run(Entity& entity, std::string_view op, const Value::List& args) {
    const auto text = [&args](std::size_t at) -> std::string {
        return at < args.size() && args[at].asString() != nullptr ? *args[at].asString() : std::string{};
    };
    const auto whole = [&args](std::size_t at, std::int64_t fallback) {
        return at < args.size() && args[at].asInt() != nullptr ? *args[at].asInt() : fallback;
    };
    if (op == "damage") {
        entity.health = std::max(0.0, entity.health - numberOf(args.empty() ? nullptr : &args[0]).value_or(0.0));
        return Value{true};
    }
    if (op == "kill") {
        entity.health = 0.0;
        entity.alive = entity.player;
        return Value{true};
    }
    if (op == "add_effect") {
        entity.effects.push_back({text(0), whole(1, 0), whole(2, 0)});
        return Value{true};
    }
    if (op == "effects") {
        Value::List rows;
        for (const Effect& effect : entity.effects) {
            rows.emplace_back(Value::Map{{"effect", Value{effect.id}},
                                         {"duration", Value{effect.ticks}},
                                         {"amplifier", Value{effect.amplifier}},
                                         {"ambient", Value{false}},
                                         {"particles", Value{true}},
                                         {"icon", Value{true}}});
        }
        return Value{std::move(rows)};
    }
    if (op == "attribute") {
        return Value{text(0).ends_with("max_health") ? 20.0 : 0.0};
    }
    if (op == "add_goal") {
        entity.goals.push_back({text(0), whole(1, 0), text(2)});
        return Value{true};
    }
    if (op == "has_goal") {
        return Value{std::ranges::any_of(entity.goals, [&text](const Goal& goal) { return goal.key == text(0); })};
    }
    if (op == "goals") {
        return rowsOfGoals(entity);
    }
    if (op == "find_path") {
        const auto axis = [&args](std::size_t at) {
            return static_cast<std::int64_t>(
                std::floor(numberOf(at < args.size() ? &args[at] : nullptr).value_or(0.0)));
        };
        return Value{Value::List{Value{axis(0)}, Value{axis(1)}, Value{axis(2)}}};
    }
    return std::nullopt;
}

void entityCall(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const GuestMemory memory = c.memory();
    const auto op = memory.string(c.i32(1), c.i32(2));
    const std::expected<Value, std::int32_t> args = c.i32(4) == 0
                                                        ? std::expected<Value, std::int32_t>{Value{Value::List{}}}
                                                        : readValue(memory, c.i32(3), c.i32(4));
    Rec rec("entity_call");
    rec.i(c.i32(0)).str(op).value(args).cap(c.i32(6));
    Entity* entity = nullptr;
    std::int32_t status = resolve(state, c.i32(0), entity);
    if (status == kOk && (!op || !args || args->asList() == nullptr)) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const std::optional<Value> answer = run(*entity, *op, *args->asList());
    c.ret(answer ? doneValue(rec, memory, c.i32(5), c.i32(6), *answer) : rec.done(kErrAbsent));
}

void entityProperties(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_properties");
    rec.i(c.i32(0)).cap(c.i32(2));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    Value::List names;
    for (const std::string_view name : kBase) {
        names.emplace_back(Value::Map{{"name", Value{std::string{name}}},
                                      {"writable", Value{name == "yaw" || name == "pitch" || name == "velocity"}}});
    }
    if (!entity->player) {
        for (const Property& property : properties()) {
            names.emplace_back(
                Value::Map{{"name", Value{std::string{property.name}}}, {"writable", Value{property.writable}}});
        }
    }
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2), Value{std::move(names)}));
}

void entityOperations(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_operations");
    rec.i(c.i32(0)).cap(c.i32(2));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    Value::List names;
    const std::array<std::pair<std::string_view, std::vector<std::string_view>>, 9> kOps{{
        {"add_effect", {}},
        {"add_goal", {}},
        {"attribute", {}},
        {"damage", {}},
        {"effects", {"effect", "duration", "amplifier", "ambient", "particles", "icon"}},
        {"find_path", {}},
        {"goals", {"key", "priority", "running", "plugin", "usable", "flags"}},
        {"has_goal", {}},
        {"kill", {}},
    }};
    for (const auto& [name, columns] : kOps) {
        Value::List cells;
        for (const std::string_view column : columns) {
            cells.emplace_back(std::string{column});
        }
        names.emplace_back(Value::Map{{"name", Value{std::string{name}}}, {"columns", Value{std::move(cells)}}});
    }
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2), Value{std::move(names)}));
}

void entitySnapshot(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_snapshot");
    rec.i(c.i32(0)).cap(c.i32(2));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    if (status != kOk || entity->player) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    const std::string saved = "nbt:" + entity->type;
    c.ret(rec.doneBuffer(c.memory(), c.i32(1), c.i32(2), saved));
}

void entityRestore(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const auto nbt = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("entity_restore");
    rec.i(c.i32(0)).str(nbt);
    Entity* entity = nullptr;
    std::int32_t status = resolve(state, c.i32(0), entity);
    if (status == kOk && (!nbt || nbt->empty() || entity->player)) {
        status = kErrInvalid;
    }
    c.ret(rec.done(status));
}

[[nodiscard]] bool readCategories(const Value* table, std::map<std::string, std::int64_t>& into) {
    if (table == nullptr) {
        return true;
    }
    if (table->asMap() == nullptr) {
        return false;
    }
    for (const std::string_view category : kCategories) {
        if (const Value* one = table->find(category)) {
            if (one->asInt() == nullptr) {
                return false;
            }
            if (*one->asInt() < 0) {
                into.erase(std::string{category});
            } else {
                into[std::string{category}] = *one->asInt();
            }
        }
    }
    return true;
}

[[nodiscard]] Value categoriesOf(const std::map<std::string, std::int64_t>& values) {
    Value::Map map;
    for (const std::string_view category : kCategories) {
        if (const auto found = values.find(std::string{category}); found != values.end()) {
            map.push_back({std::string{category}, Value{found->second}});
        }
    }
    return Value{std::move(map)};
}

void entitySpawnRules(void* u, Call& c) {
    Rules& rules = worldOf(u).areaState<Rules>("entities_rules");
    const GuestMemory memory = c.memory();
    const auto world = memory.string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> given = optionsOf(memory, c.i32(2), c.i32(3));
    Rec rec("entity_spawn_rules");
    rec.str(world);
    showOptions(rec, c.i32(3), given).cap(c.i32(5));
    if (!world || !given) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (*world != kWorld) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    if (!readCategories(given->find("limits"), rules.limits) ||
        !readCategories(given->find("ticks_per"), rules.ticksPer)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    for (const auto& [key, flag] :
         {std::pair{"allow_monsters", &rules.allowMonsters}, std::pair{"allow_animals", &rules.allowAnimals}}) {
        if (const Value* one = given->find(key)) {
            if (one->asBool() == nullptr) {
                c.ret(rec.done(kErrInvalid));
                return;
            }
            *flag = *one->asBool();
        }
    }
    c.ret(doneValue(rec, memory, c.i32(4), c.i32(5),
                    Value{Value::Map{{"limits", categoriesOf(rules.limits)},
                                     {"ticks_per", categoriesOf(rules.ticksPer)},
                                     {"allow_monsters", Value{rules.allowMonsters}},
                                     {"allow_animals", Value{rules.allowAnimals}}}}));
}

void entityEventEntity(void* u, Call& c) {
    World& world = worldOf(u);
    Rec rec("entity_event_entity");
    rec.i(c.i32(0));
    Context* context = nullptr;
    const std::int32_t status = world.checkContext(c.i32(0), ContextKind::Event, context);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const auto field = context->fields.find("entity_id");
    if (field == context->fields.end() || !std::holds_alternative<std::int64_t>(field->second)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    Entity* found = entityById(sim(world), static_cast<std::int32_t>(std::get<std::int64_t>(field->second)));
    c.ret(rec.done(found == nullptr ? kErrAbsent : handleFor(sim(world), *found)));
}

void entityAiAnswer(void* u, Call& c) {
    World& world = worldOf(u);
    Rec rec("entity_ai_answer");
    rec.i(c.i32(0)).i(c.i32(1));
    Context* context = nullptr;
    const std::int32_t status = world.checkContext(c.i32(0), ContextKind::Event, context);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    if (context->eventName != "entity.goal" && context->eventName != "entity.behavior") {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    context->fields["result"] = static_cast<std::int64_t>(c.i32(1) != 0);
    c.ret(rec.done(kOk));
}

void entityMechanic(void*, Call& c) {
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> args = optionsOf(memory, c.i32(2), c.i32(3));
    Rec rec("entity_mechanic");
    rec.str(name);
    showOptions(rec, c.i32(3), args).cap(c.i32(5));
    if (!name || !args) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (*name == "combat.fall_damage") {
        const std::optional<double> distance = numberOf(args->find("distance"));
        if (!distance) {
            c.ret(rec.done(kErrInvalid));
            return;
        }
        const double safe = numberOf(args->find("safe")).value_or(3.0);
        c.ret(
            doneValue(rec, memory, c.i32(4), c.i32(5), Value{static_cast<std::int64_t>(std::ceil(*distance - safe))}));
        return;
    }
    if (*name == "physics.projectile_margin") {
        const std::optional<double> ticks = numberOf(args->find("tick_count"));
        if (!ticks) {
            c.ret(rec.done(kErrInvalid));
            return;
        }
        c.ret(doneValue(rec, memory, c.i32(4), c.i32(5), Value{std::min(*ticks, 20.0) / 20.0 * 0.3}));
        return;
    }
    c.ret(rec.done(kErrAbsent));
}
} // namespace

void addEntitiesExtraImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"entity_get", entityGet, user},
                              {"entity_set", entitySet, user},
                              {"entity_call", entityCall, user},
                              {"entity_properties", entityProperties, user},
                              {"entity_operations", entityOperations, user},
                              {"entity_snapshot", entitySnapshot, user},
                              {"entity_restore", entityRestore, user},
                              {"entity_spawn_rules", entitySpawnRules, user},
                              {"entity_event_entity", entityEventEntity, user},
                              {"entity_ai_answer", entityAiAnswer, user},
                              {"entity_mechanic", entityMechanic, user},
                          });
}

} // namespace basalt::wire::wasm::trace
