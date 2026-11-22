#include "HostItems.hpp"

namespace basalt::wire::wasm::trace {
namespace {
using namespace items;

[[nodiscard]] Value stack(std::string_view id, std::int64_t count) {
    return Value{Value::Map{{"id", Value{std::string{id}}}, {"count", Value{count}}}};
}

[[nodiscard]] std::int64_t seedOf(const Value& spec) {
    const Value* seed = spec.find("seed");
    return seed != nullptr && seed->asInt() != nullptr ? *seed->asInt() : 0;
}

/// No spec, null and a map are accepted; the value goes to the trace the way the guest sent it.
[[nodiscard]] std::expected<Value, std::int32_t> specOf(Rec& rec, const GuestMemory& memory, std::int32_t ptr,
                                                        std::int32_t len) {
    if (len == 0) {
        rec.raw("none");
        return Value{Value::Map{}};
    }
    std::expected<Value, std::int32_t> spec = readValue(memory, ptr, len);
    rec.value(spec);
    if (!spec) {
        return spec;
    }
    if (spec->isNull()) {
        return Value{Value::Map{}};
    }
    return spec->asMap() == nullptr ? std::unexpected(kErrInvalid) : spec;
}

void lootTables(void* u, Call& c) {
    Rec rec("loot_tables");
    rec.cap(c.i32(1));
    c.ret(doneValue(rec, c.memory(), c.i32(0), c.i32(1),
                    Value{Value::List{Value{"minecraft:chests/simple_dungeon"}, Value{"minecraft:entities/zombie"}}}));
    static_cast<void>(u);
}

void lootRoll(void* u, Call& c) {
    static_cast<void>(u);
    const auto table = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("loot_roll");
    rec.str(table);
    const std::expected<Value, std::int32_t> spec = specOf(rec, c.memory(), c.i32(2), c.i32(3));
    rec.cap(c.i32(5));
    if (!table || !spec || table->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (*table != "minecraft:entities/zombie" && *table != "minecraft:chests/simple_dungeon") {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const Value rolled = *table == "minecraft:entities/zombie"
                             ? Value{Value::List{stack("minecraft:rotten_flesh", 1 + seedOf(*spec) % 2)}}
                             : Value{Value::List{stack("minecraft:bread", 2), stack("minecraft:coal", 3)}};
    c.ret(doneValue(rec, c.memory(), c.i32(4), c.i32(5), rolled));
}

void lootRollJson(void* u, Call& c) {
    static_cast<void>(u);
    const auto json = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("loot_roll_json");
    rec.str(json);
    const std::expected<Value, std::int32_t> spec = specOf(rec, c.memory(), c.i32(2), c.i32(3));
    rec.cap(c.i32(5));
    if (!json || !spec || json->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (json->front() != '{') {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    c.ret(doneValue(rec, c.memory(), c.i32(4), c.i32(5), Value{Value::List{stack("minecraft:stone", 1)}}));
}

void lootNumber(void* u, Call& c) {
    static_cast<void>(u);
    const auto json = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("loot_number");
    rec.str(json).i(c.i32(2)).i(c.i64(3)).cap(c.i32(5));
    if (!json || json->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    std::int64_t whole = 0;
    for (const char digit : *json) {
        if (digit < '0' || digit > '9') {
            c.ret(rec.done(kErrAbsent));
            return;
        }
        whole = whole * 10 + (digit - '0');
    }
    const Value answer = c.i32(2) != 0 ? Value{static_cast<double>(whole) + 0.5} : Value{whole};
    c.ret(doneValue(rec, c.memory(), c.i32(4), c.i32(5), answer));
}

[[nodiscard]] std::expected<Item, std::int32_t> itemOf(const Value& spec) {
    const Value* item = spec.find("item");
    const std::optional<Item> parsed = item == nullptr ? std::nullopt : parseItem(*item);
    return parsed ? std::expected<Item, std::int32_t>{*parsed} : std::unexpected(kErrInvalid);
}

void lootApply(void* u, Call& c) {
    static_cast<void>(u);
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(0), c.i32(1));
    Rec rec("loot_apply");
    rec.value(spec).cap(c.i32(3));
    const Value* functions = spec && spec->asMap() != nullptr ? spec->find("functions") : nullptr;
    const std::expected<Item, std::int32_t> item =
        spec && spec->asMap() != nullptr ? itemOf(*spec) : std::unexpected(kErrInvalid);
    if (!item || functions == nullptr || functions->asString() == nullptr || functions->asString()->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (functions->asString()->front() != '{' && functions->asString()->front() != '[') {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    Item made = *item;
    made.count += 1;
    c.ret(doneValue(rec, c.memory(), c.i32(2), c.i32(3), itemValue(made)));
}

void lootEnchant(void* u, Call& c) {
    static_cast<void>(u);
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(0), c.i32(1));
    Rec rec("loot_enchant");
    rec.value(spec).cap(c.i32(3));
    const std::expected<Item, std::int32_t> item =
        spec && spec->asMap() != nullptr ? itemOf(*spec) : std::unexpected(kErrInvalid);
    if (!item) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const Value* levels = spec->find("levels");
    Item made = *item;
    made.enchantments["minecraft:sharpness"] =
        1 + (levels != nullptr && levels->asInt() != nullptr ? *levels->asInt() / 10 : 0);
    c.ret(doneValue(rec, c.memory(), c.i32(2), c.i32(3), itemValue(made)));
}

void tradesRoll(void* u, Call& c) {
    static_cast<void>(u);
    const auto profession = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("trades_roll");
    rec.str(profession).i(c.i32(2));
    const std::expected<Value, std::int32_t> spec = specOf(rec, c.memory(), c.i32(3), c.i32(4));
    rec.cap(c.i32(6));
    if (!profession || !spec || profession->empty() || c.i32(2) < 1 || c.i32(2) > 5) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (*profession != "minecraft:farmer") {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const Value offer{Value::Map{{"cost_a", stack("minecraft:wheat", 20)},
                                 {"cost_b", Value{}},
                                 {"result", stack("minecraft:emerald", 1)},
                                 {"max_uses", Value{16}},
                                 {"xp", Value{2}},
                                 {"price_multiplier", Value{0.05}}}};
    c.ret(doneValue(rec, c.memory(), c.i32(5), c.i32(6), Value{Value::List{offer}}));
}
} // namespace

void addItemsLootImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {{"loot_tables", lootTables, user},
                           {"loot_roll", lootRoll, user},
                           {"loot_roll_json", lootRollJson, user},
                           {"loot_number", lootNumber, user},
                           {"loot_apply", lootApply, user},
                           {"loot_enchant", lootEnchant, user},
                           {"trades_roll", tradesRoll, user}});
}
} // namespace basalt::wire::wasm::trace
