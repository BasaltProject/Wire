#include "HostApi.hpp"

#include <array>

namespace basalt::wire::wasm::trace {
namespace {
using common::toml::TomlValue;

constexpr std::array<std::string_view, 4> kEvents = {"plugin.enable", "plugin.disable", "player.join", "player.leave"};

void log(void*, Call& c) { Rec("log").i(c.i32(0)).str(c.memory().string(c.i32(1), c.i32(2))).doneVoid(); }

void configI64(void* u, Call& c) {
    const auto key = c.memory().string(c.i32(0), c.i32(1));
    const std::int64_t fallback = c.i64(2);
    const TomlValue* value = key ? configValue(worldOf(u), *key) : nullptr;
    const std::int64_t* found = value != nullptr ? value->get<std::int64_t>() : nullptr;
    c.ret(Rec("config_i64").str(key).i(fallback).done(found != nullptr ? *found : fallback));
}

void configF64(void* u, Call& c) {
    const auto key = c.memory().string(c.i32(0), c.i32(1));
    const double fallback = c.f64(2);
    const TomlValue* value = key ? configValue(worldOf(u), *key) : nullptr;
    double result = fallback;
    if (value != nullptr) {
        if (const double* real = value->get<double>()) {
            result = *real;
        } else if (const std::int64_t* whole = value->get<std::int64_t>()) {
            result = static_cast<double>(*whole);
        }
    }
    c.ret(Rec("config_f64").str(key).f64(fallback).done(result));
}

void configBool(void* u, Call& c) {
    const auto key = c.memory().string(c.i32(0), c.i32(1));
    const std::int32_t fallback = c.i32(2);
    const TomlValue* value = key ? configValue(worldOf(u), *key) : nullptr;
    const bool* found = value != nullptr ? value->get<bool>() : nullptr;
    const std::int32_t result = found != nullptr ? (*found ? 1 : 0) : fallback;
    c.ret(Rec("config_bool").str(key).i(fallback).done(result));
}

void configStr(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto key = memory.string(c.i32(0), c.i32(1));
    Rec rec("config_str");
    rec.str(key).cap(c.i32(3));
    const TomlValue* value = key ? configValue(worldOf(u), *key) : nullptr;
    if (!key) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::string* text = value != nullptr ? value->get<std::string>() : nullptr;
    if (text == nullptr) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    c.ret(rec.doneBuffer(memory, c.i32(2), c.i32(3), *text));
}

void translate(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto key = memory.string(c.i32(0), c.i32(1));
    const auto args = readArgs(memory, c.i32(2), c.i32(3));
    Rec rec("translate");
    rec.str(key).args(args).cap(c.i32(5));
    if (!key || !args) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::optional<std::string> found = langTemplate(worldOf(u), *key);
    if (!found) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    c.ret(rec.doneBuffer(memory, c.i32(4), c.i32(5), substitute(*found, *args)));
}

void eventOn(void* u, Call& c) {
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    const std::int32_t priority = c.i32(2);
    const std::int32_t token = c.i32(3);
    Rec rec("event_on");
    rec.str(name).i(priority).i(token);
    bool known = false;
    for (const std::string_view event : kEvents) {
        known = known || (name && event == *name);
    }
    if (!known || priority < 0 || priority > 4) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    worldOf(u).listeners.push_back(Listener{std::string{*name}, priority, token});
    c.ret(rec.done(kOk));
}

void eventPlayer(void* u, Call& c) {
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Event, context);
    c.ret(Rec("event_player").i(c.i32(0)).done(status != kOk ? status : context->player));
}

void eventFieldStr(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto field = memory.string(c.i32(1), c.i32(2));
    Rec rec("event_field_str");
    rec.i(c.i32(0)).str(field).cap(c.i32(4));
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Event, context);
    if (status != kOk || !field) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    const auto found = context->fields.find(std::string{*field});
    if (found == context->fields.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const std::string* text = std::get_if<std::string>(&found->second);
    c.ret(text == nullptr ? rec.done(kErrInvalid) : rec.doneBuffer(memory, c.i32(3), c.i32(4), *text));
}

void eventFieldI64(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto field = memory.string(c.i32(1), c.i32(2));
    Rec rec("event_field_i64");
    rec.i(c.i32(0)).str(field);
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Event, context);
    if (status != kOk || !field) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    const auto found = context->fields.find(std::string{*field});
    if (found == context->fields.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const std::int64_t* number = std::get_if<std::int64_t>(&found->second);
    if (number == nullptr || !memory.write(c.i32(3), *number)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    c.ret(rec.done(kOk, "out=" + std::to_string(*number)));
}

void eventCancel(void* u, Call& c) {
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Event, context);
    if (status == kOk) {
        context->cancelled = c.i32(1) != 0;
    }
    c.ret(Rec("event_cancel").i(c.i32(0)).i(c.i32(1)).done(status));
}

void taskAfter(void* u, Call& c) {
    World& world = worldOf(u);
    Rec rec("task_after");
    rec.i(c.i32(0)).i(c.i32(1));
    if (c.i32(0) < 0) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const auto handle = static_cast<std::int32_t>(world.tasks.size() + 1);
    world.tasks.push_back(Task{handle, c.i32(1), world.now + c.i32(0), 0, true});
    c.ret(rec.done(handle));
}

void taskEvery(void* u, Call& c) {
    World& world = worldOf(u);
    Rec rec("task_every");
    rec.i(c.i32(0)).i(c.i32(1)).i(c.i32(2));
    if (c.i32(0) < 0 || c.i32(1) <= 0) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const auto handle = static_cast<std::int32_t>(world.tasks.size() + 1);
    world.tasks.push_back(Task{handle, c.i32(2), world.now + c.i32(0), c.i32(1), true});
    c.ret(rec.done(handle));
}

void taskCancel(void* u, Call& c) {
    World& world = worldOf(u);
    const std::int32_t handle = c.i32(0);
    std::int32_t status = kErrAbsent;
    if (handle >= 1 && static_cast<std::size_t>(handle) <= world.tasks.size()) {
        Task& task = world.tasks[static_cast<std::size_t>(handle - 1)];
        if (task.alive) {
            task.alive = false;
            status = kOk;
        }
    }
    c.ret(Rec("task_cancel").i(handle).done(status));
}
} // namespace

void addCoreImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"log", log, user},
                              {"config_i64", configI64, user},
                              {"config_f64", configF64, user},
                              {"config_bool", configBool, user},
                              {"config_str", configStr, user},
                              {"translate", translate, user},
                              {"event_on", eventOn, user},
                              {"event_player", eventPlayer, user},
                              {"event_field_str", eventFieldStr, user},
                              {"event_field_i64", eventFieldI64, user},
                              {"event_cancel", eventCancel, user},
                              {"task_after", taskAfter, user},
                              {"task_every", taskEvery, user},
                              {"task_cancel", taskCancel, user},
                          });
}

} // namespace basalt::wire::wasm::trace
