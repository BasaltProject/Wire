#include "HostApi.hpp"

namespace basalt::wire::wasm::trace {
namespace {
void deliver(void* u, Call& c, std::string_view name, std::string_view channel) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto text = memory.string(c.i32(1), c.i32(2));
    const auto args = readArgs(memory, c.i32(3), c.i32(4));
    Rec rec(name);
    rec.i(c.i32(0)).str(text).args(args);
    Player* player = nullptr;
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!text || !args)) {
        status = kErrInvalid;
    }
    std::string effect;
    if (status == kOk) {
        const std::optional<std::string> rendered = render(world, *text, *args);
        if (rendered) {
            effect = "world " + std::string{channel} + " " + player->name + " " + quote(*rendered);
        } else {
            status = kErrAbsent;
        }
    }
    c.ret(rec.done(status));
    if (!effect.empty()) {
        emitLine(effect);
    }
}

void playerSend(void* u, Call& c) { deliver(u, c, "player_send", "send"); }

void playerActionBar(void* u, Call& c) { deliver(u, c, "player_action_bar", "action-bar"); }

void playerParticle(void* u, Call& c) {
    World& world = worldOf(u);
    const auto id = c.memory().string(c.i32(1), c.i32(2));
    Player* player = nullptr;
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!id || id->empty() || c.i32(3) < 0)) {
        status = kErrInvalid;
    }
    c.ret(Rec("player_particle").i(c.i32(0)).str(id).i(c.i32(3)).f64(c.f64(4)).done(status));
    if (status == kOk) {
        emitLine("world particle " + player->name + " " + quote(*id) + " count=" + std::to_string(c.i32(3)) +
                 " spread=" + number(c.f64(4)));
    }
}

void playerSound(void* u, Call& c) {
    World& world = worldOf(u);
    const auto id = c.memory().string(c.i32(1), c.i32(2));
    Player* player = nullptr;
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!id || id->empty())) {
        status = kErrInvalid;
    }
    c.ret(Rec("player_sound").i(c.i32(0)).str(id).f32(c.f32(3)).f32(c.f32(4)).done(status));
    if (status == kOk) {
        emitLine("world sound " + player->name + " " + quote(*id) + " volume=" + number(c.f32(3)) +
                 " pitch=" + number(c.f32(4)));
    }
}

void commandRegister(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto spec = memory.string(c.i32(0), c.i32(1));
    const auto permission = memory.string(c.i32(2), c.i32(3));
    Rec rec("command_register");
    rec.str(spec).str(permission).i(c.i32(4));
    if (!spec || !permission) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    auto parsed = plugins::CommandSpec::parse(
        *spec, permission->empty() ? std::nullopt : std::optional<std::string>{std::string{*permission}});
    if (!parsed) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    worldOf(u).commands.push_back(RegisteredCommand{std::move(*parsed), c.i32(4)});
    c.ret(rec.done(kOk));
}

void commandSourceName(void* u, Call& c) {
    Context* context = nullptr;
    Rec rec("command_source_name");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Command, context);
    c.ret(status != kOk ? rec.done(status) : rec.doneBuffer(c.memory(), c.i32(1), c.i32(2), context->source));
}

void commandIsConsole(void* u, Call& c) {
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Command, context);
    c.ret(Rec("command_is_console").i(c.i32(0)).done(status != kOk ? status : (context->console ? 1 : 0)));
}

void commandReply(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto text = memory.string(c.i32(1), c.i32(2));
    const auto args = readArgs(memory, c.i32(3), c.i32(4));
    Rec rec("command_reply");
    rec.i(c.i32(0)).str(text).args(args);
    Context* context = nullptr;
    std::int32_t status = world.checkContext(c.i32(0), ContextKind::Command, context);
    if (status == kOk && (!text || !args)) {
        status = kErrInvalid;
    }
    std::string effect;
    if (status == kOk) {
        const std::optional<std::string> rendered = render(world, *text, *args);
        if (rendered) {
            effect = "world reply " + context->source + " " + quote(*rendered);
        } else {
            status = kErrAbsent;
        }
    }
    c.ret(rec.done(status));
    if (!effect.empty()) {
        emitLine(effect);
    }
}

[[nodiscard]] const plugins::CommandValue* argumentOf(const Context& context, std::string_view name,
                                                      const plugins::CommandArgument*& spec) {
    for (std::size_t index = 0; index < context.spec.arguments.size() && index < context.values.size(); ++index) {
        if (context.spec.arguments[index].name == name) {
            spec = &context.spec.arguments[index];
            return &context.values[index];
        }
    }
    return nullptr;
}

void commandArgStr(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    Rec rec("command_arg_str");
    rec.i(c.i32(0)).str(name).cap(c.i32(4));
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Command, context);
    if (status != kOk || !name) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    const plugins::CommandArgument* spec = nullptr;
    const plugins::CommandValue* value = argumentOf(*context, *name, spec);
    if (value == nullptr || std::holds_alternative<std::monostate>(*value)) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    std::string text;
    if (const auto* word = std::get_if<std::string>(value)) {
        text = *word;
    } else if (const auto* whole = std::get_if<std::int64_t>(value)) {
        text = std::to_string(*whole);
    } else if (const auto* real = std::get_if<double>(value)) {
        text = number(*real);
    } else {
        text = std::get<bool>(*value) ? "true" : "false";
    }
    c.ret(rec.doneBuffer(memory, c.i32(3), c.i32(4), text));
}

void commandArgI64(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    Rec rec("command_arg_i64");
    rec.i(c.i32(0)).str(name);
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Command, context);
    if (status != kOk || !name) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    const plugins::CommandArgument* spec = nullptr;
    const plugins::CommandValue* value = argumentOf(*context, *name, spec);
    if (value == nullptr || std::holds_alternative<std::monostate>(*value)) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const auto* whole = std::get_if<std::int64_t>(value);
    if (whole == nullptr || !memory.write(c.i32(3), *whole)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    c.ret(rec.done(kOk, "out=" + std::to_string(*whole)));
}

void menuCreate(void* u, Call& c) {
    World& world = worldOf(u);
    const auto title = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("menu_create");
    rec.str(title).i(c.i32(2));
    if (!title || c.i32(2) < 1 || c.i32(2) > 6) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    world.menus.push_back(Menu{std::string{*title}, c.i32(2), {}});
    c.ret(rec.done(static_cast<std::int32_t>(world.menus.size())));
}

void menuSet(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto item = memory.string(c.i32(2), c.i32(3));
    const auto name = memory.string(c.i32(4), c.i32(5));
    const auto lore = memory.string(c.i32(6), c.i32(7));
    Rec rec("menu_set");
    rec.i(c.i32(0)).i(c.i32(1)).str(item).str(name).str(lore).i(c.i32(8));
    const std::int32_t handle = c.i32(0);
    if (handle < 1 || static_cast<std::size_t>(handle) > world.menus.size() || !item || !name || !lore) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    Menu& menu = world.menus[static_cast<std::size_t>(handle - 1)];
    if (c.i32(1) < 0 || c.i32(1) >= menu.rows * 9 || item->empty()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    menu.slots[c.i32(1)] = MenuSlot{std::string{*item}, std::string{*name}, std::string{*lore}, c.i32(8)};
    c.ret(rec.done(kOk));
}

void menuOpen(void* u, Call& c) {
    World& world = worldOf(u);
    Rec rec("menu_open");
    rec.i(c.i32(0)).i(c.i32(1));
    const std::int32_t handle = c.i32(0);
    if (handle < 1 || static_cast<std::size_t>(handle) > world.menus.size()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(world, c.i32(1), player);
    c.ret(rec.done(status));
    if (status == kOk) {
        const Menu& menu = world.menus[static_cast<std::size_t>(handle - 1)];
        emitLine("world menu " + std::to_string(handle) + " opens for " + player->name + " title=" + quote(menu.title) +
                 " rows=" + std::to_string(menu.rows));
        for (const auto& [slot, entry] : menu.slots) {
            emitLine("world menu-slot " + std::to_string(slot) + " item=" + quote(entry.item) +
                     " name=" + quote(entry.name) + " lore=" + quote(entry.lore));
        }
    }
}

void menuClickPlayer(void* u, Call& c) {
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::MenuClick, context);
    c.ret(Rec("menu_click_player").i(c.i32(0)).done(status != kOk ? status : context->player));
}
} // namespace

void addUiImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"player_send", playerSend, user},
                              {"player_action_bar", playerActionBar, user},
                              {"player_particle", playerParticle, user},
                              {"player_sound", playerSound, user},
                              {"command_register", commandRegister, user},
                              {"command_source_name", commandSourceName, user},
                              {"command_is_console", commandIsConsole, user},
                              {"command_reply", commandReply, user},
                              {"command_arg_str", commandArgStr, user},
                              {"command_arg_i64", commandArgI64, user},
                              {"menu_create", menuCreate, user},
                              {"menu_set", menuSet, user},
                              {"menu_open", menuOpen, user},
                              {"menu_click_player", menuClickPlayer, user},
                          });
}

} // namespace basalt::wire::wasm::trace
