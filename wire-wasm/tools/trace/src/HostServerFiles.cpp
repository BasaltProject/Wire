#include "HostServerState.hpp"

#include <algorithm>
#include <set>

namespace basalt::wire::wasm::trace {
namespace {
using namespace server;

struct Resolved {
    std::int32_t status = kOk;
    std::string relative;
};

[[nodiscard]] std::string normalized(std::string_view path) {
    std::string out{path};
    while (out.starts_with("./")) {
        out.erase(0, 2);
    }
    while (out.size() > 1 && out.ends_with('/')) {
        out.pop_back();
    }
    return out == "." ? std::string{} : out;
}

[[nodiscard]] bool safe(std::string_view relative) {
    return relative.find('\0') == std::string_view::npos && relative.find('\\') == std::string_view::npos &&
           relative.find("..") == std::string_view::npos && relative.find(':') == std::string_view::npos &&
           !relative.starts_with('/');
}

[[nodiscard]] bool grants(const World& world, std::string_view relative) {
    return std::any_of(world.capabilities.files.begin(), world.capabilities.files.end(), [relative](std::string entry) {
        while (entry.ends_with('/')) {
            entry.pop_back();
        }
        if (entry.empty() || entry == "." || entry == "*") {
            return true;
        }
        return relative == entry || (relative.starts_with(entry) && relative[entry.size()] == '/');
    });
}

[[nodiscard]] Resolved resolve(const World& world, std::optional<std::string_view> path) {
    Resolved out;
    if (!granted(world, "files")) {
        out.status = kErrDenied;
        return out;
    }
    if (!path || !safe(normalized(*path))) {
        out.status = kErrInvalid;
        return out;
    }
    out.relative = normalized(*path);
    if (!grants(world, out.relative)) {
        out.status = kErrDenied;
    }
    return out;
}

[[nodiscard]] bool isDirectory(const State& state, std::string_view relative) {
    const std::string prefix = std::string{relative} + "/";
    return std::any_of(state.files.begin(), state.files.end(),
                       [&prefix](const auto& file) { return file.first.starts_with(prefix); });
}

void filesRead(void* u, Call& c) {
    World& world = worldOf(u);
    const auto path = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("files_read");
    rec.str(path).cap(c.i32(3));
    const Resolved at = resolve(world, path);
    if (at.status != kOk) {
        c.ret(rec.done(at.status));
        return;
    }
    State& state = stateOf(world);
    auto found = state.files.find(at.relative);
    if (found != state.files.end()) {
        c.ret(rec.doneBuffer(c.memory(), c.i32(2), c.i32(3), found->second));
        return;
    }
    found = state.packaged.find(at.relative);
    c.ret(found != state.packaged.end() ? rec.doneBuffer(c.memory(), c.i32(2), c.i32(3), found->second)
                                        : rec.done(kErrAbsent));
}

void filesWrite(void* u, Call& c) {
    World& world = worldOf(u);
    const auto path = c.memory().string(c.i32(0), c.i32(1));
    const auto data = c.memory().string(c.i32(2), c.i32(3));
    Rec rec("files_write");
    rec.str(path).str(data);
    const Resolved at = resolve(world, path);
    if (at.status != kOk || !data || at.relative.empty()) {
        c.ret(rec.done(at.status != kOk ? at.status : kErrInvalid));
        return;
    }
    stateOf(world).files[at.relative] = std::string{*data};
    c.ret(rec.done(kOk));
}

void filesExists(void* u, Call& c) {
    World& world = worldOf(u);
    const auto path = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("files_exists");
    rec.str(path);
    const Resolved at = resolve(world, path);
    State& state = stateOf(world);
    c.ret(rec.done(at.status != kOk ? at.status
                                    : (state.files.contains(at.relative) || state.packaged.contains(at.relative) ||
                                               isDirectory(state, at.relative)
                                           ? 1
                                           : 0)));
}

void filesList(void* u, Call& c) {
    World& world = worldOf(u);
    const auto given = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("files_list");
    rec.str(given).cap(c.i32(3));
    if (!granted(world, "files") || !given || !safe(normalized(*given))) {
        c.ret(rec.done(!granted(world, "files") ? kErrDenied : kErrInvalid));
        return;
    }
    const std::string prefix = normalized(*given);
    State& state = stateOf(world);
    std::set<std::string> names;
    for (const auto* source : {&state.packaged, &state.files}) {
        for (const auto& [relative, bytes] : *source) {
            if (relative.starts_with(prefix) && grants(world, relative)) {
                names.insert(relative);
            }
        }
    }
    c.ret(doneValue(rec, c, 2, listOfText({names.begin(), names.end()})));
}

void filesStat(void* u, Call& c) {
    World& world = worldOf(u);
    const auto path = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("files_stat");
    rec.str(path).cap(c.i32(3));
    const Resolved at = resolve(world, path);
    if (at.status != kOk) {
        c.ret(rec.done(at.status));
        return;
    }
    State& state = stateOf(world);
    const auto found = state.files.find(at.relative);
    if (found != state.files.end()) {
        c.ret(doneValue(rec, c, 2,
                        mapOf({{"kind", Value{"file"}},
                               {"size", Value{static_cast<std::int64_t>(found->second.size())}},
                               {"modified", Value{std::int64_t{1700000000000}}}})));
    } else if (isDirectory(state, at.relative) || at.relative.empty()) {
        c.ret(doneValue(
            rec, c, 2,
            mapOf(
                {{"kind", Value{"directory"}}, {"size", Value{0}}, {"modified", Value{std::int64_t{1700000000000}}}})));
    } else {
        c.ret(rec.done(kErrAbsent));
    }
}

void filesWalk(void* u, Call& c) {
    World& world = worldOf(u);
    const auto path = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("files_walk");
    rec.str(path).cap(c.i32(3));
    const Resolved at = resolve(world, path);
    State& state = stateOf(world);
    if (at.status != kOk) {
        c.ret(rec.done(at.status));
        return;
    }
    if (!at.relative.empty() && !isDirectory(state, at.relative)) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const std::string base = at.relative.empty() ? std::string{} : at.relative + "/";
    std::set<std::string> entries;
    for (const auto& [relative, bytes] : state.files) {
        if (!relative.starts_with(base)) {
            continue;
        }
        const std::string rest = relative.substr(base.size());
        for (std::size_t slash = rest.find('/'); slash != std::string::npos; slash = rest.find('/', slash + 1)) {
            entries.insert(rest.substr(0, slash + 1));
        }
        entries.insert(rest);
    }
    c.ret(doneValue(rec, c, 2, listOfText({entries.begin(), entries.end()})));
}

void filesDelete(void* u, Call& c) {
    World& world = worldOf(u);
    const auto path = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("files_delete");
    rec.str(path).i(c.i32(2));
    const Resolved at = resolve(world, path);
    if (at.status != kOk || at.relative.empty()) {
        c.ret(rec.done(at.status != kOk ? at.status : kErrInvalid));
        return;
    }
    State& state = stateOf(world);
    if (state.files.erase(at.relative) > 0) {
        c.ret(rec.done(kOk));
        return;
    }
    if (!isDirectory(state, at.relative)) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    if (c.i32(2) == 0) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const std::string prefix = at.relative + "/";
    std::erase_if(state.files, [&prefix](const auto& file) { return file.first.starts_with(prefix); });
    c.ret(rec.done(kOk));
}

void filesCapabilities(void* u, Call& c) {
    World& world = worldOf(u);
    const auto path = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("files_capabilities");
    rec.str(path).cap(c.i32(3));
    const Resolved at = resolve(world, path);
    State& state = stateOf(world);
    if (at.status != kOk) {
        c.ret(rec.done(at.status));
        return;
    }
    if (!at.relative.empty() && !isDirectory(state, at.relative)) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    c.ret(doneValue(rec, c, 2, mapOf({{"atomic_move", Value{true}}, {"hard_links", Value{true}}})));
}

void storeGet(void* u, Call& c) {
    World& world = worldOf(u);
    const auto key = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("store_get");
    rec.str(key).cap(c.i32(3));
    if (!granted(world, "storage") || !key || key->empty()) {
        c.ret(rec.done(!granted(world, "storage") ? kErrDenied : kErrInvalid));
        return;
    }
    const auto found = stateOf(world).store.find(std::string{*key});
    c.ret(found == stateOf(world).store.end() ? rec.done(kErrAbsent) : doneValue(rec, c, 2, found->second));
}

void storeSet(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto key = memory.string(c.i32(0), c.i32(1));
    Rec rec("store_set");
    rec.str(key);
    const std::expected<Value, std::int32_t> value = readValue(memory, c.i32(2), c.i32(3));
    rec.value(value);
    if (!granted(world, "storage")) {
        c.ret(rec.done(kErrDenied));
        return;
    }
    if (!key || key->empty() || !value || value->asList() != nullptr || value->asMap() != nullptr ||
        value->asBytes() != nullptr) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (value->isNull()) {
        stateOf(world).store.erase(std::string{*key});
    } else {
        stateOf(world).store[std::string{*key}] = *value;
    }
    c.ret(rec.done(kOk));
}

[[nodiscard]] std::optional<std::string> jsonPath(std::optional<std::string_view> name) {
    if (!name || name->empty() || name->starts_with('/')) {
        return std::nullopt;
    }
    const bool isPath = name->find('/') != std::string_view::npos || name->ends_with(".json");
    const std::string relative = isPath ? std::string{*name} : "data/" + std::string{*name} + ".json";
    return safe(relative) ? std::optional<std::string>{relative} : std::nullopt;
}

void dataRead(void* u, Call& c) {
    World& world = worldOf(u);
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("data_read");
    rec.str(name).cap(c.i32(3));
    if (!granted(world, "storage")) {
        c.ret(rec.done(kErrDenied));
        return;
    }
    const std::optional<std::string> path = jsonPath(name);
    if (!path) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    const auto found = stateOf(world).files.find(*path);
    if (found == stateOf(world).files.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const auto parsed = common::parseJson(found->second);
    c.ret(parsed ? doneValue(rec, c, 2, fromJson(*parsed)) : rec.done(kErrInvalid));
}

void dataWrite(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(0), c.i32(1));
    Rec rec("data_write");
    rec.str(name);
    const std::expected<Value, std::int32_t> value = readValue(memory, c.i32(2), c.i32(3));
    rec.value(value);
    if (!granted(world, "storage")) {
        c.ret(rec.done(kErrDenied));
        return;
    }
    const std::optional<std::string> path = jsonPath(name);
    const std::optional<common::JsonValue> json = value ? toJson(*value) : std::nullopt;
    if (!path || !json) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    stateOf(world).files[*path] = common::writeJson(*json);
    c.ret(rec.done(kOk));
}

[[nodiscard]] std::string swapped(std::string_view key) {
    std::string out{key};
    for (char& letter : out) {
        letter = letter == '-' ? '_' : letter == '_' ? '-' : letter;
    }
    return out;
}

void configValueImport(void* u, Call& c) {
    World& world = worldOf(u);
    const auto key = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("config_value");
    rec.str(key).cap(c.i32(3));
    if (!key) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (key->empty()) {
        c.ret(doneValue(rec, c, 2, fromToml(common::toml::TomlValue::ofTable(world.config))));
        return;
    }
    const common::toml::TomlValue* found = configValue(world, *key);
    if (found == nullptr) {
        found = configValue(world, swapped(*key));
    }
    c.ret(found != nullptr ? doneValue(rec, c, 2, fromToml(*found)) : rec.done(kErrAbsent));
}
} // namespace

void addServerFilesImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"files_read", filesRead, user},
                              {"files_write", filesWrite, user},
                              {"files_exists", filesExists, user},
                              {"files_list", filesList, user},
                              {"files_stat", filesStat, user},
                              {"files_walk", filesWalk, user},
                              {"files_delete", filesDelete, user},
                              {"files_capabilities", filesCapabilities, user},
                              {"store_get", storeGet, user},
                              {"store_set", storeSet, user},
                              {"data_read", dataRead, user},
                              {"data_write", dataWrite, user},
                              {"config_value", configValueImport, user},
                          });
}

} // namespace basalt::wire::wasm::trace
