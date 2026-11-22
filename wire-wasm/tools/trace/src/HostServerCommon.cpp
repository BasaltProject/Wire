#include "HostServerState.hpp"

#include <algorithm>

namespace basalt::wire::wasm::trace::server {
namespace {
using common::JsonObject;
using common::JsonValue;
using common::toml::TomlTable;
using common::toml::TomlValue;

[[nodiscard]] Value queryOf(World& world, std::string_view kind, std::string_view subject, std::string_view node) {
    const Player* player = subject == "console" ? nullptr : world.playerByName(subject);
    return mapOf({{"kind", Value{std::string{kind}}},
                  {"name", Value{subject == "console" ? std::string{"CONSOLE"} : std::string{subject}}},
                  {"uuid", Value{player != nullptr ? uuidOfPlayer(*player) : std::string{}}},
                  {"console", Value{subject == "console"}},
                  {"node", Value{std::string{node}}},
                  {"world", Value{player != nullptr ? std::string{"world"} : std::string{}}}});
}

[[nodiscard]] std::optional<std::string> ask(World& world, const Provider& provider, std::string_view kind,
                                             std::string_view subject, std::string_view argument) {
    Context context;
    context.kind = ContextKind::Completion;
    context.value = queryOf(world, kind, subject, argument);
    context.result = std::string{};
    if (world.invoke(provider.token, std::move(context)) != kOk) {
        return std::nullopt;
    }
    return world.current.result;
}

[[nodiscard]] std::vector<Provider> byPriority(const State& state, std::int32_t kind) {
    std::vector<Provider> ordered;
    for (const Provider& provider : state.providers) {
        if ((provider.kinds & kind) != 0) {
            ordered.push_back(provider);
        }
    }
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const Provider& a, const Provider& b) { return a.priority > b.priority; });
    return ordered;
}
} // namespace

Value fromToml(const TomlValue& value) {
    if (const std::string* text = value.get<std::string>()) {
        return Value{*text};
    }
    if (const std::int64_t* whole = value.get<std::int64_t>()) {
        return Value{*whole};
    }
    if (const double* real = value.get<double>()) {
        return Value{*real};
    }
    if (const bool* flag = value.get<bool>()) {
        return Value{*flag};
    }
    if (const auto* array = value.get<std::vector<TomlValue>>()) {
        Value::List list;
        for (const TomlValue& item : *array) {
            list.push_back(fromToml(item));
        }
        return Value{std::move(list)};
    }
    Value::Map map;
    if (const TomlTable* table = value.get<TomlTable>()) {
        for (const std::string& key : table->keys()) {
            map.push_back(MapEntry{key, fromToml(*table->find(key))});
        }
    }
    return Value{std::move(map)};
}

Value fromJson(const JsonValue& value) {
    if (value.isNull()) {
        return Value{};
    }
    if (const bool* flag = value.get<bool>()) {
        return Value{*flag};
    }
    if (const std::int64_t* whole = value.get<std::int64_t>()) {
        return Value{*whole};
    }
    if (const double* real = value.get<double>()) {
        return Value{*real};
    }
    if (const std::string* text = value.get<std::string>()) {
        return Value{*text};
    }
    if (const auto* array = value.get<std::vector<JsonValue>>()) {
        Value::List list;
        for (const JsonValue& item : *array) {
            list.push_back(fromJson(item));
        }
        return Value{std::move(list)};
    }
    Value::Map map;
    if (const JsonObject* object = value.get<JsonObject>()) {
        for (std::size_t index = 0; index < object->size(); ++index) {
            map.push_back(MapEntry{object->names()[index], fromJson(object->values()[index])});
        }
    }
    return Value{std::move(map)};
}

std::optional<JsonValue> toJson(const Value& value) {
    if (value.isNull()) {
        return JsonValue{};
    }
    if (const bool* flag = value.asBool()) {
        return JsonValue::of(*flag);
    }
    if (const std::int64_t* whole = value.asInt()) {
        return JsonValue::of(*whole);
    }
    if (const double* real = value.asFloat()) {
        return JsonValue::of(*real);
    }
    if (const std::string* text = value.asString()) {
        return JsonValue::of(*text);
    }
    if (const Value::List* list = value.asList()) {
        std::vector<JsonValue> items;
        for (const Value& item : *list) {
            std::optional<JsonValue> converted = toJson(item);
            if (!converted) {
                return std::nullopt;
            }
            items.push_back(std::move(*converted));
        }
        return JsonValue::ofArray(std::move(items));
    }
    const Value::Map* map = value.asMap();
    if (map == nullptr) {
        return std::nullopt;
    }
    JsonObject object;
    for (const MapEntry& entry : *map) {
        std::optional<JsonValue> converted = toJson(entry.value);
        if (!converted) {
            return std::nullopt;
        }
        object.set(entry.key, std::move(*converted));
    }
    return JsonValue::ofObject(std::move(object));
}

std::string uuidOfPlayer(const Player& player) {
    std::string digits = std::to_string(player.handle);
    return "00000000-0000-0000-0000-" + std::string(12 - digits.size(), '0') + digits;
}

std::int32_t chain(World& world, std::string_view subject, std::string_view node) {
    State& state = stateOf(world);
    const bool console = subject == "console";
    const Player* player = console ? nullptr : world.playerByName(subject);
    if (!console && player == nullptr) {
        return -1;
    }
    if (!state.inside && !world.current.live) {
        for (const Provider& provider : byPriority(state, 1)) {
            const std::optional<std::string> answer = ask(world, provider, "check", subject, node);
            if (answer && !answer->empty() && (*answer)[0] != '-') {
                return (*answer)[0] == '1' ? 1 : 0;
            }
        }
    }
    if (console) {
        return 1;
    }
    const auto own = state.grants.find(std::string{subject});
    if (own != state.grants.end()) {
        if (const auto found = own->second.find(std::string{node}); found != own->second.end()) {
            return found->second ? 1 : 0;
        }
    }
    return node.starts_with("basalt.use") ? 1 : 0;
}

std::optional<std::string> chainMeta(World& world, std::string_view subject, std::string_view key) {
    State& state = stateOf(world);
    const bool console = subject == "console";
    if (!console && world.playerByName(subject) == nullptr) {
        return std::nullopt;
    }
    if (!state.inside && !world.current.live) {
        for (const Provider& provider : byPriority(state, 2)) {
            const std::optional<std::string> answer = ask(world, provider, "meta", subject, key);
            if (answer && !answer->empty() && (*answer)[0] == '=') {
                return answer->substr(1);
            }
        }
    }
    if (!console && key == "prefix") {
        return std::string{"[Member]"};
    }
    return std::nullopt;
}

bool granted(const World& world, std::string_view capability) { return world.capabilities.has(capability); }
} // namespace basalt::wire::wasm::trace::server

namespace basalt::wire::wasm::trace {
void addServerCommonImports(World&, std::vector<HostImport>&) {}
} // namespace basalt::wire::wasm::trace
