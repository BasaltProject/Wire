#include "HostEventsState.hpp"
#include "Scenario.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace basalt::wire::wasm::trace {
namespace {
constexpr std::int32_t kFlagPost = 1;
constexpr std::int32_t kFlagCancelled = 2;
constexpr std::int32_t kMaxEmitDepth = 8;

constexpr EventRow kRows[] = {
#include "HostEventsTable.inc"
};

[[nodiscard]] bool isPluginEvent(std::string_view name) { return name.find(':') != std::string_view::npos; }

[[nodiscard]] std::string_view fieldNameOf(std::string_view field) {
    if (field == "name") {
        return "player_name";
    }
    if (field == "uuid") {
        return "player_uuid";
    }
    return field;
}

[[nodiscard]] bool isScalar(const Value& value) {
    return value.isNull() || value.asBool() != nullptr || value.asInt() != nullptr || value.asFloat() != nullptr ||
           value.asString() != nullptr;
}

[[nodiscard]] std::int32_t onlineCount(const World& world) {
    return static_cast<std::int32_t>(
        std::count_if(world.players.begin(), world.players.end(), [](const Player& p) { return p.online; }));
}

[[nodiscard]] std::int32_t eventContext(World& world, std::int32_t id, Context*& out) {
    return world.checkContext(id, ContextKind::Event, out);
}

void eventOnEx(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(0), c.i32(1));
    const std::int32_t priority = c.i32(2);
    const std::int32_t flags = c.i32(3);
    Rec rec("event_on_ex");
    rec.str(name).i(priority).i(flags);
    const auto filters =
        c.i32(5) == 0 ? std::expected<Value, std::int32_t>{Value{Value::Map{}}} : readValue(memory, c.i32(4), c.i32(5));
    rec.raw(c.i32(5) == 0 ? "none" : (filters ? canonical(*filters) : "<invalid>")).i(c.i32(6));
    if (!name || priority < 0 || priority > 4 || (flags & ~(kFlagPost | kFlagCancelled)) != 0 || !filters ||
        filters->asMap() == nullptr) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const bool plugin = isPluginEvent(*name);
    const EventRow* row = plugin ? nullptr : eventRow(*name);
    if (plugin ? (flags != 0 || !filters->asMap()->empty()) : (row == nullptr)) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (row != nullptr) {
        const std::vector<std::string> fields = splitList(row->fields);
        bool valid = (flags & kFlagCancelled) == 0 || row->cancellable;
        for (const MapEntry& entry : *filters->asMap()) {
            valid = valid && std::find(fields.begin(), fields.end(), entry.key) != fields.end() &&
                    isScalar(entry.value) && !entry.value.isNull();
        }
        if (!valid) {
            c.ret(rec.done(kErrInvalid));
            return;
        }
    }
    eventsOf(world).listeners.push_back(
        EventListener{std::string{*name}, priority, flags, *filters->asMap(), c.i32(6)});
    c.ret(rec.done(kOk));
}

void eventName(void* u, Call& c) {
    Context* context = nullptr;
    Rec rec("event_name");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = eventContext(worldOf(u), c.i32(0), context);
    c.ret(status != kOk ? rec.done(status) : rec.doneBuffer(c.memory(), c.i32(1), c.i32(2), context->eventName));
}

void eventGet(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    Rec rec("event_get");
    rec.i(c.i32(0)).str(name).cap(c.i32(4));
    Context* context = nullptr;
    const std::int32_t status = eventContext(world, c.i32(0), context);
    if (status != kOk || !name) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    Value stored;
    const Value* found = nullptr;
    if (*name == "online" && context->value && field(*context->value, "player_uuid") != nullptr) {
        stored = Value{static_cast<std::int64_t>(onlineCount(world))};
        found = &stored;
    } else if (context->value && !isPluginEvent(context->eventName)) {
        found = field(*context->value, fieldNameOf(*name));
    }
    if (found == nullptr) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    c.ret(doneValue(rec, memory, c.i32(3), c.i32(4), *found));
}

[[nodiscard]] std::optional<Value> fitted(const Value& current, const Value& given) {
    if (given.isNull() || current.isNull() || current.data.index() == given.data.index()) {
        return given;
    }
    if (current.asFloat() != nullptr && given.asInt() != nullptr) {
        return Value{static_cast<double>(*given.asInt())};
    }
    if (current.asInt() != nullptr && given.asFloat() != nullptr && std::trunc(*given.asFloat()) == *given.asFloat()) {
        return Value{static_cast<std::int64_t>(*given.asFloat())};
    }
    return std::nullopt;
}

void eventSet(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    const auto given = readValue(memory, c.i32(3), c.i32(4));
    Rec rec("event_set");
    rec.i(c.i32(0)).str(name).value(given);
    Context* context = nullptr;
    const std::int32_t status = eventContext(world, c.i32(0), context);
    if (status != kOk || !name || !given) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    const EventRow* row = isPluginEvent(context->eventName) ? nullptr : eventRow(context->eventName);
    const std::vector<std::string> mutables = row != nullptr ? splitList(row->mutables) : std::vector<std::string>{};
    Value* slot = context->value ? field(*context->value, *name) : nullptr;
    if (row == nullptr || eventsOf(world).post || slot == nullptr || !isScalar(*given) ||
        std::find(mutables.begin(), mutables.end(), *name) == mutables.end()) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::optional<Value> stored = fitted(*slot, *given);
    if (!stored) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    *slot = *stored;
    c.ret(rec.done(kOk));
}

void eventFields(void* u, Call& c) {
    Context* context = nullptr;
    Rec rec("event_fields");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = eventContext(worldOf(u), c.i32(0), context);
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const Value fields = isPluginEvent(context->eventName) || !context->value ? Value{Value::Map{}} : *context->value;
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2), fields));
}

void eventDescribe(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(0), c.i32(1));
    Rec rec("event_describe");
    rec.str(name).cap(c.i32(3));
    if (!name) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const EventRow* row = eventRow(*name);
    const bool custom = isPluginEvent(*name) &&
                        std::any_of(eventsOf(worldOf(u)).listeners.begin(), eventsOf(worldOf(u)).listeners.end(),
                                    [&](const EventListener& l) { return l.event == *name; });
    if (row == nullptr && !custom) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    Value::List fields;
    if (row != nullptr) {
        const std::vector<std::string> mutables = splitList(row->mutables);
        for (const std::string& field : splitList(row->fields)) {
            fields.emplace_back(
                Value::Map{{"name", Value{field}},
                           {"mutable", Value{std::find(mutables.begin(), mutables.end(), field) != mutables.end()}}});
        }
    }
    const Value described{Value::Map{{"name", Value{std::string{*name}}},
                                     {"cancellable", Value{row != nullptr && row->cancellable}},
                                     {"custom", Value{row == nullptr}},
                                     {"fields", Value{std::move(fields)}}}};
    c.ret(doneValue(rec, memory, c.i32(2), c.i32(3), described));
}

void eventNames(void*, Call& c) {
    Rec rec("event_names");
    rec.cap(c.i32(1));
    Value::List names;
    for (const EventRow& row : kRows) {
        names.emplace_back(std::string{row.name});
    }
    const std::int32_t result = putValue(c.memory(), c.i32(0), c.i32(1), Value{std::move(names)});
    c.ret(result >= 0 && result <= c.i32(1) ? rec.done(result, std::to_string(std::size(kRows)) + " names")
                                            : rec.done(result));
}

void eventCancelled(void* u, Call& c) {
    Context* context = nullptr;
    const std::int32_t status = eventContext(worldOf(u), c.i32(0), context);
    c.ret(Rec("event_cancelled").i(c.i32(0)).done(status != kOk ? status : (context->cancelled ? 1 : 0)));
}

void eventIsPost(void* u, Call& c) {
    Context* context = nullptr;
    const std::int32_t status = eventContext(worldOf(u), c.i32(0), context);
    c.ret(Rec("event_is_post").i(c.i32(0)).done(status != kOk ? status : (eventsOf(worldOf(u)).post ? 1 : 0)));
}

void eventCancelWith(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto text = memory.string(c.i32(1), c.i32(2));
    const auto args = readArgs(memory, c.i32(3), c.i32(4));
    Rec rec("event_cancel_with");
    rec.i(c.i32(0)).str(text).args(args);
    Context* context = nullptr;
    std::int32_t status = eventContext(world, c.i32(0), context);
    const EventRow* row = status == kOk && !isPluginEvent(context->eventName) ? eventRow(context->eventName) : nullptr;
    if (status == kOk && (!text || !args || row == nullptr || !row->cancellable || eventsOf(world).post)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        const std::optional<std::string> shown = render(world, *text, *args);
        if (shown) {
            context->cancelled = true;
            eventsOf(world).cancelMessage = *shown;
        } else {
            status = kErrAbsent;
        }
    }
    c.ret(rec.done(status));
}

void eventCancelMessage(void* u, Call& c) {
    World& world = worldOf(u);
    Context* context = nullptr;
    Rec rec("event_cancel_message");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = eventContext(world, c.i32(0), context);
    if (status != kOk || !eventsOf(world).cancelMessage) {
        c.ret(rec.done(status != kOk ? status : kErrAbsent));
        return;
    }
    c.ret(rec.doneBuffer(c.memory(), c.i32(1), c.i32(2), *eventsOf(world).cancelMessage));
}

void eventPayload(void* u, Call& c) {
    Context* context = nullptr;
    Rec rec("event_payload");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = eventContext(worldOf(u), c.i32(0), context);
    if (status != kOk || !context->value || !isPluginEvent(context->eventName)) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    c.ret(doneValue(rec, c.memory(), c.i32(1), c.i32(2), *context->value));
}

void eventEmit(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(0), c.i32(1));
    const auto payload = readValue(memory, c.i32(2), c.i32(3));
    Rec rec("event_emit");
    rec.str(name).value(payload);
    if (!name || !isPluginEvent(*name) || !payload || eventsOf(world).emitDepth >= kMaxEmitDepth) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    // The only plugin here is the emitter, which never hears itself while it is inside a callback.
    c.ret(rec.done(0));
}

/// Parses `key=value` words; a value in double quotes may hold spaces. Nothing when a word is not an assignment.
[[nodiscard]] std::optional<std::vector<std::pair<std::string, Value>>> assignments(std::string_view rest) {
    std::vector<std::pair<std::string, Value>> out;
    std::size_t at = 0;
    while (at < rest.size()) {
        while (at < rest.size() && rest[at] == ' ') {
            ++at;
        }
        if (at >= rest.size()) {
            break;
        }
        const std::size_t equals = rest.find('=', at);
        if (equals == std::string_view::npos) {
            return std::nullopt;
        }
        std::string key{rest.substr(at, equals - at)};
        at = equals + 1;
        std::string text;
        bool quoted = false;
        if (at < rest.size() && rest[at] == '"') {
            const std::size_t close = rest.find('"', at + 1);
            if (close == std::string_view::npos) {
                return std::nullopt;
            }
            text = std::string{rest.substr(at + 1, close - at - 1)};
            at = close + 1;
            quoted = true;
        } else {
            const std::size_t end = rest.find(' ', at);
            text = std::string{rest.substr(at, end == std::string_view::npos ? end : end - at)};
            at = end == std::string_view::npos ? rest.size() : end;
        }
        Value value{text};
        if (!quoted) {
            char* end = nullptr;
            if (text == "true" || text == "false") {
                value = Value{text == "true"};
            } else if (text == "null") {
                value = Value{};
            } else if (const long long whole = std::strtoll(text.c_str(), &end, 10); !text.empty() && *end == '\0') {
                value = Value{static_cast<std::int64_t>(whole)};
            } else if (const double real = std::strtod(text.c_str(), &end); !text.empty() && *end == '\0') {
                value = Value{real};
            }
        }
        out.emplace_back(std::move(key), std::move(value));
    }
    return out;
}

[[nodiscard]] bool matches(const EventListener& listener, const Value& fields, bool cancelled) {
    if (cancelled && (listener.flags & kFlagCancelled) == 0) {
        return false;
    }
    for (const MapEntry& wanted : listener.filters) {
        Value copy = fields;
        const Value* have = field(copy, wanted.key);
        if (have == nullptr || !(*have == wanted.value)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::vector<EventListener> ordered(const World& world, std::string_view name) {
    std::vector<EventListener> matching;
    for (const EventListener& listener : const_cast<World&>(world).areaState<EventsTrace>("events").listeners) {
        if (listener.event == name) {
            matching.push_back(listener);
        }
    }
    std::stable_sort(matching.begin(), matching.end(), [](const EventListener& a, const EventListener& b) {
        const bool postA = (a.flags & kFlagPost) != 0;
        const bool postB = (b.flags & kFlagPost) != 0;
        return postA != postB ? !postA : a.priority < b.priority;
    });
    return matching;
}

bool fire(World& world, std::string_view rest) {
    const std::size_t split = rest.find(' ');
    const std::string name{rest.substr(0, split)};
    const auto given = assignments(split == std::string_view::npos ? std::string_view{} : rest.substr(split + 1));
    const EventRow* row = eventRow(name);
    if (!given || row == nullptr) {
        emitLine("world event-unknown " + name);
        return given.has_value();
    }
    Value::Map fields;
    for (const std::string& key : splitList(row->fields)) {
        fields.push_back({key, Value{}});
    }
    Value event{std::move(fields)};
    std::int32_t player = 0;
    for (const auto& [key, value] : *given) {
        if (key == "player") {
            Player* found = value.asString() != nullptr ? world.playerByName(*value.asString()) : nullptr;
            if (found == nullptr) {
                emitLine("world event-rejected player " + canonical(value));
                return true;
            }
            player = found->handle;
            if (Value* slot = field(event, "player_name")) {
                *slot = Value{found->name};
            }
            if (Value* slot = field(event, "player_uuid")) {
                *slot = Value{uuidOf(*found)};
            }
        } else if (Value* slot = field(event, key)) {
            *slot = value;
        } else {
            emitLine("world event-rejected field " + quote(key));
            return true;
        }
    }
    EventsTrace& state = eventsOf(world);
    state.cancelMessage.reset();
    bool cancelled = false;
    std::int32_t delivered = 0;
    for (const EventListener& listener : ordered(world, name)) {
        const bool post = (listener.flags & kFlagPost) != 0;
        if (!matches(listener, event, cancelled)) {
            continue;
        }
        Context context;
        context.kind = ContextKind::Event;
        context.player = player;
        context.eventName = name;
        context.value = event;
        context.cancelled = cancelled;
        state.post = post;
        (void)world.invoke(listener.token, std::move(context));
        state.post = false;
        ++delivered;
        if (!post && world.current.value) {
            event = *world.current.value;
            cancelled = world.current.cancelled;
        }
    }
    emitLine("world event " + name + " listeners=" + std::to_string(delivered) +
             " cancelled=" + (cancelled ? "1" : "0"));
    emitLine("world event-fields " + canonical(event));
    if (state.cancelMessage) {
        emitLine("world event-cancel-message " + quote(*state.cancelMessage));
    }
    return true;
}

bool emitFromOther(World& world, std::string_view rest) {
    const std::size_t split = rest.find(' ');
    const std::string name{rest.substr(0, split)};
    const auto given = assignments(split == std::string_view::npos ? std::string_view{} : rest.substr(split + 1));
    if (!given || !isPluginEvent(name)) {
        return false;
    }
    Value::Map payload;
    for (const auto& [key, value] : *given) {
        payload.push_back({key, value});
    }
    const Value document{std::move(payload)};
    std::int32_t heard = 0;
    EventsTrace& state = eventsOf(world);
    for (const EventListener& listener : ordered(world, name)) {
        Context context;
        context.kind = ContextKind::Event;
        context.eventName = name;
        context.value = document;
        state.post = false;
        (void)world.invoke(listener.token, std::move(context));
        ++heard;
    }
    emitLine("world emitted " + name + " heard=" + std::to_string(heard));
    return true;
}

const ScenarioVerbRegistrar kFire{"fire_event", fire};
const ScenarioVerbRegistrar kEmit{"emit_event", emitFromOther};
} // namespace

const std::vector<EventRow>& eventRows() {
    static const std::vector<EventRow> rows{std::begin(kRows), std::end(kRows)};
    return rows;
}

const EventRow* eventRow(std::string_view name) {
    for (const EventRow& row : kRows) {
        if (row.name == name) {
            return &row;
        }
    }
    return nullptr;
}

std::vector<std::string> splitList(std::string_view text) {
    std::vector<std::string> parts;
    while (!text.empty()) {
        const std::size_t comma = text.find(',');
        parts.emplace_back(text.substr(0, comma));
        text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
    }
    return parts;
}

void addEventsImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"event_on_ex", eventOnEx, user},
                              {"event_name", eventName, user},
                              {"event_get", eventGet, user},
                              {"event_set", eventSet, user},
                              {"event_fields", eventFields, user},
                              {"event_describe", eventDescribe, user},
                              {"event_names", eventNames, user},
                              {"event_cancelled", eventCancelled, user},
                              {"event_cancel_with", eventCancelWith, user},
                              {"event_cancel_message", eventCancelMessage, user},
                              {"event_is_post", eventIsPost, user},
                              {"event_emit", eventEmit, user},
                              {"event_payload", eventPayload, user},
                          });
}

} // namespace basalt::wire::wasm::trace
