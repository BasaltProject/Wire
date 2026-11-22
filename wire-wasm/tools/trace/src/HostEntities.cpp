#include "HostEntitiesState.hpp"

namespace basalt::wire::wasm::trace {
namespace {
using namespace entitiesSim;

void entityOfPlayer(void* u, Call& c) {
    World& world = worldOf(u);
    Sim& state = sim(world);
    Rec rec("entity_of_player");
    rec.i(c.i32(0));
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    for (const Entity& entity : state.entities) {
        if (entity.player && entity.playerHandle == player->handle) {
            c.ret(rec.done(handleFor(state, entity)));
            return;
        }
    }
    Entity entity;
    entity.id = kPlayerEntityBase + player->handle;
    entity.uuid = uuidOf(entity.id);
    entity.type = "minecraft:player";
    entity.name = player->name;
    entity.player = true;
    entity.playerHandle = player->handle;
    state.entities.push_back(entity);
    c.ret(rec.done(handleFor(state, state.entities.back())));
}

void entityPlayer(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_player");
    rec.i(c.i32(0));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    c.ret(rec.done(status != kOk ? status : entity->player ? entity->playerHandle : kErrAbsent));
}

void entityRelease(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_release");
    rec.i(c.i32(0));
    Entity* entity = nullptr;
    const std::int32_t handle = c.i32(0);
    if (resolve(state, handle, entity) != kOk) {
        c.ret(rec.done(kErrStale));
        return;
    }
    state.handles[static_cast<std::size_t>(handle - 1)] = 0;
    c.ret(rec.done(kOk));
}

void entityAlive(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_alive");
    rec.i(c.i32(0));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    c.ret(rec.done(status == kOk ? 1 : status == kErrStale ? 0 : status));
}

void entityId(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_id");
    rec.i(c.i32(0));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    c.ret(rec.done(status != kOk ? status : entity->id));
}

void entityById(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const auto world = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("entity_by_id");
    rec.str(world).i(c.i32(2));
    Entity* found = world && *world == kWorld ? entityById(state, c.i32(2)) : nullptr;
    c.ret(rec.done(found == nullptr ? kErrAbsent : handleFor(state, *found)));
}

void entityByUuid(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const auto uuid = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("entity_by_uuid");
    rec.str(uuid);
    if (!uuid || !looksLikeUuid(*uuid)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    for (Entity& entity : state.entities) {
        if (entity.alive && entity.uuid == *uuid) {
            c.ret(rec.done(handleFor(state, entity)));
            return;
        }
    }
    c.ret(rec.done(kErrAbsent));
}

void entityQuery(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const GuestMemory memory = c.memory();
    const auto world = memory.string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> query = optionsOf(memory, c.i32(2), c.i32(3));
    Rec rec("entity_query");
    rec.str(world);
    showOptions(rec, c.i32(3), query).cap(c.i32(5));
    if (!world || !query) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (*world != kWorld) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    std::vector<std::string> types;
    if (const Value* type = query->find("type")) {
        if (const std::string* one = type->asString()) {
            types.push_back(withNamespace(*one));
        } else if (const Value::List* many = type->asList()) {
            for (const Value& item : *many) {
                if (item.asString() == nullptr) {
                    c.ret(rec.done(kErrInvalid));
                    return;
                }
                types.push_back(withNamespace(*item.asString()));
            }
        }
    }
    const std::optional<double> radius = numberOf(query->find("radius"));
    const std::array<std::optional<double>, 3> center{numberOf(query->find("x")), numberOf(query->find("y")),
                                                      numberOf(query->find("z"))};
    const std::optional<double> limit = numberOf(query->find("limit"));
    std::vector<std::int32_t> handles;
    for (Entity& entity : state.entities) {
        if (!entity.alive || (!types.empty() && std::ranges::find(types, entity.type) == types.end())) {
            continue;
        }
        if (radius && center[0] && center[1] && center[2]) {
            const double dx = entity.pos[0] - *center[0];
            const double dy = entity.pos[1] - *center[1];
            const double dz = entity.pos[2] - *center[2];
            if (dx * dx + dy * dy + dz * dz > *radius * *radius) {
                continue;
            }
        }
        if (limit && handles.size() >= static_cast<std::size_t>(*limit)) {
            break;
        }
        handles.push_back(handleFor(state, entity));
    }
    c.ret(doneHandles(rec, memory, c.i32(4), c.i32(5), handles));
}

void entityTypes(void* u, Call& c) {
    static_cast<void>(u);
    Rec rec("entity_types");
    rec.cap(c.i32(1));
    Value::List ids;
    for (const std::string_view type : kTypes) {
        ids.emplace_back(std::string{type});
    }
    c.ret(doneValue(rec, c.memory(), c.i32(0), c.i32(1), Value{std::move(ids)}));
}

void entityTypeAttributes(void* u, Call& c) {
    static_cast<void>(u);
    const auto type = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("entity_type_attributes");
    rec.str(type).cap(c.i32(3));
    if (!type || std::ranges::find(kTypes, withNamespace(*type)) == kTypes.end()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    Value::Map attributes;
    if (*type != "minecraft:item") {
        attributes = {{"minecraft:max_health", Value{20.0}}, {"minecraft:movement_speed", Value{0.25}}};
    }
    c.ret(doneValue(rec, c.memory(), c.i32(2), c.i32(3), Value{std::move(attributes)}));
}

void entitySpawn(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const GuestMemory memory = c.memory();
    const auto world = memory.string(c.i32(0), c.i32(1));
    const auto type = memory.string(c.i32(2), c.i32(3));
    const std::expected<Value, std::int32_t> options = optionsOf(memory, c.i32(7), c.i32(8));
    Rec rec("entity_spawn");
    rec.str(world).str(type).f64(c.f64(4)).f64(c.f64(5)).f64(c.f64(6));
    showOptions(rec, c.i32(8), options);
    if (!world || !type || type->empty() || !options || *world != kWorld ||
        std::ranges::find(kTypes, withNamespace(*type)) == kTypes.end()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const Value* reason = options->find("reason");
    if (reason != nullptr && reason->asString() != nullptr && *reason->asString() == "veto") {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    Entity entity;
    entity.id = state.nextId++;
    entity.uuid = uuidOf(entity.id);
    entity.type = withNamespace(*type);
    entity.pos = {c.f64(4), c.f64(5), c.f64(6)};
    entity.yaw = numberOf(options->find("yaw")).value_or(0.0);
    state.entities.push_back(entity);
    c.ret(rec.done(handleFor(state, state.entities.back())));
}

void entitySpawnNbt(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const GuestMemory memory = c.memory();
    const auto world = memory.string(c.i32(0), c.i32(1));
    const auto nbt = memory.string(c.i32(2), c.i32(3));
    const auto cause = memory.string(c.i32(4), c.i32(5));
    Rec rec("entity_spawn_nbt");
    rec.str(world).raw("nbt[" + std::to_string(c.i32(3)) + "]").str(cause);
    if (!world || !nbt || !cause || nbt->empty() || *world != kWorld) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    Entity entity;
    entity.id = state.nextId++;
    entity.uuid = uuidOf(entity.id);
    entity.type = "minecraft:pig";
    state.entities.push_back(entity);
    c.ret(rec.done(handleFor(state, state.entities.back())));
}

void entityDropItem(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const GuestMemory memory = c.memory();
    const auto world = memory.string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> item = readValue(memory, c.i32(2), c.i32(3));
    Rec rec("entity_drop_item");
    rec.str(world).value(item).f64(c.f64(4)).f64(c.f64(5)).f64(c.f64(6)).f64(c.f64(7)).f64(c.f64(8)).f64(c.f64(9));
    const Value* id = item ? item->find("id") : nullptr;
    if (!world || *world != kWorld || !item || item->asMap() == nullptr ||
        (id == nullptr && item->find("nbt") == nullptr) || (id != nullptr && id->asString() == nullptr)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    Entity entity;
    entity.id = state.nextId++;
    entity.uuid = uuidOf(entity.id);
    entity.type = "minecraft:item";
    entity.pos = {c.f64(4), c.f64(5), c.f64(6)};
    entity.velocity = {c.f64(7), c.f64(8), c.f64(9)};
    state.entities.push_back(entity);
    c.ret(rec.done(handleFor(state, state.entities.back())));
}

void entityRemove(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_remove");
    rec.i(c.i32(0));
    Entity* entity = nullptr;
    std::int32_t status = resolve(state, c.i32(0), entity);
    if (status == kOk && entity->player) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        entity->alive = false;
    }
    c.ret(rec.done(status));
}

void entityRemoveUuid(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const auto world = c.memory().string(c.i32(0), c.i32(1));
    const auto uuid = c.memory().string(c.i32(2), c.i32(3));
    Rec rec("entity_remove_uuid");
    rec.str(world).str(uuid);
    if (!world || *world != kWorld || !uuid || !looksLikeUuid(*uuid)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    for (Entity& entity : state.entities) {
        if (entity.alive && !entity.player && entity.uuid == *uuid) {
            entity.alive = false;
            c.ret(rec.done(kOk));
            return;
        }
    }
    c.ret(rec.done(kErrAbsent));
}

void entityPosition(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_position");
    rec.i(c.i32(0));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const bool wrote = c.memory().write(c.i32(1), entity->pos);
    c.ret(wrote ? rec.done(kOk, "[" + number(entity->pos[0]) + ", " + number(entity->pos[1]) + ", " +
                                    number(entity->pos[2]) + "]")
                : rec.done(kErrInvalid));
}

void entityTeleport(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    const std::expected<Value, std::int32_t> options = optionsOf(c.memory(), c.i32(4), c.i32(5));
    Rec rec("entity_teleport");
    rec.i(c.i32(0)).f64(c.f64(1)).f64(c.f64(2)).f64(c.f64(3));
    showOptions(rec, c.i32(5), options);
    Entity* entity = nullptr;
    std::int32_t status = resolve(state, c.i32(0), entity);
    if (status == kOk && !options) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const Value* world = options->find("world");
    if (world != nullptr && world->asString() != nullptr && *world->asString() != kWorld) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    entity->pos = {c.f64(1), c.f64(2), c.f64(3)};
    entity->yaw = numberOf(options->find("yaw")).value_or(entity->yaw);
    entity->pitch = numberOf(options->find("pitch")).value_or(entity->pitch);
    c.ret(rec.done(kOk));
}

void entityVelocity(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_velocity");
    rec.i(c.i32(0));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    if (status != kOk || entity->player) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    const bool wrote = c.memory().write(c.i32(1), entity->velocity);
    c.ret(wrote ? rec.done(kOk, "[" + number(entity->velocity[0]) + ", " + number(entity->velocity[1]) + ", " +
                                    number(entity->velocity[2]) + "]")
                : rec.done(kErrInvalid));
}

void entitySetVelocity(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_set_velocity");
    rec.i(c.i32(0)).f64(c.f64(1)).f64(c.f64(2)).f64(c.f64(3));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    if (status != kOk || entity->player) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    entity->velocity = {c.f64(1), c.f64(2), c.f64(3)};
    c.ret(rec.done(kOk));
}

void entityRotation(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_rotation");
    rec.i(c.i32(0));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const std::array<double, 2> facing{entity->yaw, entity->pitch};
    c.ret(c.memory().write(c.i32(1), facing) ? rec.done(kOk, "[" + number(facing[0]) + ", " + number(facing[1]) + "]")
                                             : rec.done(kErrInvalid));
}

void entitySetRotation(void* u, Call& c) {
    Sim& state = sim(worldOf(u));
    Rec rec("entity_set_rotation");
    rec.i(c.i32(0)).f64(c.f64(1)).f64(c.f64(2));
    Entity* entity = nullptr;
    const std::int32_t status = resolve(state, c.i32(0), entity);
    if (status == kOk) {
        entity->yaw = c.f64(1);
        entity->pitch = c.f64(2);
    }
    c.ret(rec.done(status));
}
} // namespace

void addEntitiesImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"entity_of_player", entityOfPlayer, user},
                              {"entity_player", entityPlayer, user},
                              {"entity_release", entityRelease, user},
                              {"entity_alive", entityAlive, user},
                              {"entity_id", entityId, user},
                              {"entity_by_id", entityById, user},
                              {"entity_by_uuid", entityByUuid, user},
                              {"entity_query", entityQuery, user},
                              {"entity_types", entityTypes, user},
                              {"entity_type_attributes", entityTypeAttributes, user},
                              {"entity_spawn", entitySpawn, user},
                              {"entity_spawn_nbt", entitySpawnNbt, user},
                              {"entity_drop_item", entityDropItem, user},
                              {"entity_remove", entityRemove, user},
                              {"entity_remove_uuid", entityRemoveUuid, user},
                              {"entity_position", entityPosition, user},
                              {"entity_teleport", entityTeleport, user},
                              {"entity_velocity", entityVelocity, user},
                              {"entity_set_velocity", entitySetVelocity, user},
                              {"entity_rotation", entityRotation, user},
                              {"entity_set_rotation", entitySetRotation, user},
                          });
}

} // namespace basalt::wire::wasm::trace
