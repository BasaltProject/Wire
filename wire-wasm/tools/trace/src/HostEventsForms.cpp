#include "HostEventsState.hpp"
#include "Scenario.hpp"

#include <cstdlib>

namespace basalt::wire::wasm::trace {
namespace {
[[nodiscard]] std::optional<std::string> resolved(const World& world, std::string_view source) {
    return render(world, source, ArgList{});
}

[[nodiscard]] std::optional<std::string_view> textIn(const Value& map, std::string_view key) {
    const Value* found = map.find(key);
    const std::string* text = found == nullptr ? nullptr : found->asString();
    return text == nullptr ? std::nullopt : std::optional<std::string_view>{*text};
}

[[nodiscard]] bool textsValid(const World& world, const Value& map, std::string_view key, bool resolve,
                              std::size_t& count) {
    const Value* found = map.find(key);
    if (found == nullptr) {
        return true;
    }
    const auto one = [&](const Value& item) {
        const std::string* text = item.asString();
        ++count;
        return text != nullptr && (!resolve || resolved(world, *text).has_value());
    };
    if (const Value::List* list = found->asList()) {
        for (const Value& item : *list) {
            if (!one(item)) {
                return false;
            }
        }
        return true;
    }
    return one(*found);
}

[[nodiscard]] std::optional<double> numberIn(const Value& map, std::string_view key) {
    const Value* found = map.find(key);
    if (found == nullptr) {
        return std::nullopt;
    }
    if (const double* real = found->asFloat()) {
        return *real;
    }
    if (const std::int64_t* whole = found->asInt()) {
        return static_cast<double>(*whole);
    }
    return std::nullopt;
}

[[nodiscard]] bool fieldValid(const World& world, const Value& item) {
    if (item.asMap() == nullptr) {
        return false;
    }
    const std::optional<std::string_view> key = textIn(item, "key");
    if (!key || key->empty() || !resolved(world, textIn(item, "label").value_or(*key))) {
        return false;
    }
    const std::string_view kind = textIn(item, "type").value_or("text");
    if (kind == "slider") {
        const double low = numberIn(item, "min").value_or(0.0);
        const double high = numberIn(item, "max").value_or(1.0);
        return (item.find("min") == nullptr || numberIn(item, "min")) &&
               (item.find("max") == nullptr || numberIn(item, "max")) && high >= low;
    }
    if (kind == "choice") {
        std::size_t options = 0;
        return textsValid(world, item, "options", false, options) && options > 0;
    }
    return kind == "text" || kind == "toggle";
}

[[nodiscard]] bool formValid(const World& world, const Value& form) {
    std::size_t counted = 0;
    if (form.asMap() == nullptr || !resolved(world, textIn(form, "title").value_or("")) ||
        !textsValid(world, form, "body", true, counted) || !textsValid(world, form, "buttons", true, counted)) {
        return false;
    }
    if (const Value* fields = form.find("fields")) {
        const Value::List* list = fields->asList();
        if (list == nullptr || list->size() > 64) {
            return false;
        }
        for (const Value& item : *list) {
            if (!fieldValid(world, item)) {
                return false;
            }
        }
    }
    return true;
}

void formOpen(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto form = readValue(memory, c.i32(1), c.i32(2));
    Rec rec("form_open");
    rec.i(c.i32(0)).value(form).i(c.i32(3));
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    if (!form || !formValid(world, *form)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    EventsTrace& state = eventsOf(world);
    if (const auto open = state.forms.find(player->name); open != state.forms.end()) {
        (void)cancelRequest(world, open->second.request);
    }
    const std::int32_t request = beginRequest(world, c.i32(3));
    state.forms[player->name] = FormState{request, *form};
    emitLine("world form " + player->name + " " + canonical(*form));
    c.ret(rec.done(request));
}

void formClose(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    Rec rec("form_close");
    rec.i(c.i32(0));
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    EventsTrace& state = eventsOf(world);
    const auto open = state.forms.find(player->name);
    if (open == state.forms.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    (void)cancelRequest(world, open->second.request);
    state.forms.erase(open);
    emitLine("world form-closed " + player->name);
    c.ret(rec.done(kOk));
}

void dialogShow(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    const auto dialog = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("dialog_show");
    rec.i(c.i32(0)).str(dialog).i(c.i32(3));
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk && (!dialog || dialog->empty())) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    if (dialog->starts_with('{') && !dialog->ends_with('}')) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    EventsTrace& state = eventsOf(world);
    if (const auto waiting = state.dialogs.find(player->name); waiting != state.dialogs.end()) {
        (void)cancelRequest(world, waiting->second);
    }
    const std::int32_t request = beginRequest(world, c.i32(3));
    state.dialogs[player->name] = request;
    emitLine("world dialog " + player->name + " " + quote(*dialog));
    c.ret(rec.done(request));
}

void dialogClear(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    Rec rec("dialog_clear");
    rec.i(c.i32(0));
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk) {
        EventsTrace& state = eventsOf(world);
        if (const auto waiting = state.dialogs.find(player->name); waiting != state.dialogs.end()) {
            (void)cancelRequest(world, waiting->second);
            state.dialogs.erase(waiting);
        }
        emitLine("world dialog-cleared " + player->name);
    }
    c.ret(rec.done(status));
}

void playerOpenInventory(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto kind = memory.string(c.i32(1), c.i32(2));
    Rec rec("player_open_inventory");
    rec.i(c.i32(0)).str(kind);
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    std::expected<Value, std::int32_t> spec = Value{Value::Map{}};
    if (c.i32(4) != 0) {
        spec = readValue(memory, c.i32(3), c.i32(4));
    }
    rec.value(spec);
    const auto whole = [&](std::string_view key) { return spec && spec->find(key) && spec->find(key)->asInt(); };
    bool valid = spec && spec->asMap() != nullptr && kind;
    if (valid && *kind == "merchant") {
        valid = textIn(*spec, "title") && textIn(*spec, "offers");
    } else if (valid && *kind == "trades") {
        valid = whole("entity");
    } else if (valid && *kind == "sign") {
        valid = whole("x") && whole("y") && whole("z");
    } else if (valid && *kind == "book") {
        valid = textIn(*spec, "item").has_value();
    } else {
        valid = false;
    }
    if (!valid) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    emitLine("world window " + player->name + " " + std::string{*kind});
    c.ret(rec.done(kOk));
}

void playerCloseWindow(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    Rec rec("player_close_window");
    rec.i(c.i32(0));
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status == kOk) {
        emitLine("world window-closed " + player->name);
    }
    c.ret(rec.done(status));
}

[[nodiscard]] const Value* fieldSpec(const Value& form, const std::string& key) {
    const Value* fields = form.find("fields");
    const Value::List* list = fields == nullptr ? nullptr : fields->asList();
    if (list == nullptr) {
        return nullptr;
    }
    for (const Value& item : *list) {
        if (textIn(item, "key").value_or("") == key) {
            return &item;
        }
    }
    return nullptr;
}

/// answer_form <player> <button> [key=text ...]: the player submits the form they have open.
bool answerForm(World& world, std::string_view rest) {
    const std::size_t split = rest.find(' ');
    const std::string name{rest.substr(0, split)};
    std::string_view tail = split == std::string_view::npos ? std::string_view{} : rest.substr(split + 1);
    const std::size_t next = tail.find(' ');
    const std::string button{tail.substr(0, next)};
    tail = next == std::string_view::npos ? std::string_view{} : tail.substr(next + 1);
    Player* player = world.playerByName(name);
    EventsTrace& state = eventsOf(world);
    const auto open = state.forms.find(name);
    if (player == nullptr || open == state.forms.end() || button.empty()) {
        emitLine("world answer-rejected " + name);
        return true;
    }
    Value::Map answers;
    while (!tail.empty()) {
        const std::size_t end = tail.find(' ');
        const std::string_view word = tail.substr(0, end);
        tail = end == std::string_view::npos ? std::string_view{} : tail.substr(end + 1);
        const std::size_t equals = word.find('=');
        if (equals == std::string_view::npos) {
            return false;
        }
        const std::string key{word.substr(0, equals)};
        const std::string text{word.substr(equals + 1)};
        const Value* spec = fieldSpec(open->second.form, key);
        if (spec == nullptr) {
            continue;
        }
        const std::string_view kind = textIn(*spec, "type").value_or("text");
        if (kind == "toggle") {
            answers.push_back({key, Value{text == "true"}});
        } else if (kind == "slider") {
            answers.push_back({key, Value{std::strtod(text.c_str(), nullptr)}});
        } else {
            answers.push_back({key, Value{text}});
        }
    }
    const std::int32_t request = open->second.request;
    state.forms.erase(open);
    const bool delivered = completeRequest(
        world, request,
        Value{Value::Map{{"player", Value{static_cast<std::int64_t>(player->handle)}},
                         {"button", Value{static_cast<std::int64_t>(std::strtoll(button.c_str(), nullptr, 10))}},
                         {"values", Value{std::move(answers)}}}});
    emitLine(std::string{"world form-answered "} + name + (delivered ? "" : " (nobody was waiting)"));
    return true;
}

/// click_dialog <player> <id> [payload]: the player answers the dialog they have open.
bool clickDialog(World& world, std::string_view rest) {
    const std::size_t first = rest.find(' ');
    const std::string name{rest.substr(0, first)};
    std::string_view tail = first == std::string_view::npos ? std::string_view{} : rest.substr(first + 1);
    const std::size_t second = tail.find(' ');
    const std::string id{tail.substr(0, second)};
    const std::string payload{second == std::string_view::npos ? std::string_view{} : tail.substr(second + 1)};
    Player* player = world.playerByName(name);
    EventsTrace& state = eventsOf(world);
    const auto waiting = state.dialogs.find(name);
    if (player == nullptr || waiting == state.dialogs.end() || id.empty()) {
        emitLine("world click-rejected " + name);
        return true;
    }
    const std::int32_t request = waiting->second;
    state.dialogs.erase(waiting);
    const bool delivered =
        completeRequest(world, request,
                        Value{Value::Map{{"player", Value{static_cast<std::int64_t>(player->handle)}},
                                         {"id", Value{id}},
                                         {"payload", Value{payload}}}});
    emitLine(std::string{"world dialog-answered "} + name + (delivered ? "" : " (nobody was waiting)"));
    return true;
}

const ScenarioVerbRegistrar kAnswer{"answer_form", answerForm};
const ScenarioVerbRegistrar kClick{"click_dialog", clickDialog};
} // namespace

void addEventsFormsImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"form_open", formOpen, user},
                              {"form_close", formClose, user},
                              {"dialog_show", dialogShow, user},
                              {"dialog_clear", dialogClear, user},
                              {"player_open_inventory", playerOpenInventory, user},
                              {"player_close_window", playerCloseWindow, user},
                          });
}

} // namespace basalt::wire::wasm::trace
