#include "HostApi.hpp"

#include <algorithm>
#include <cstring>

namespace basalt::wire::wasm::trace {
namespace {
[[nodiscard]] std::map<std::string, DataValue>* dataOf(World& world, Player* player) {
    return world.playersDataGranted && player != nullptr ? &world.data[player->name] : nullptr;
}

void playerByName(void* u, Call& c) {
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Player* player = name ? worldOf(u).playerByName(*name) : nullptr;
    c.ret(Rec("player_by_name").str(name).done(player != nullptr ? player->handle : 0));
}

void playersOnline(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    std::vector<std::int32_t> handles;
    for (const Player& player : worldOf(u).players) {
        if (player.online) {
            handles.push_back(player.handle);
        }
    }
    const auto count = static_cast<std::int32_t>(handles.size());
    std::string list = "[";
    for (std::size_t index = 0; index < handles.size(); ++index) {
        list += (index == 0 ? "" : ",") + std::to_string(handles[index]);
    }
    list += "]";
    Rec rec("players_online");
    rec.cap(c.i32(1));
    if (c.i32(1) < 0) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (count > c.i32(1)) {
        c.ret(rec.done(count));
        return;
    }
    const auto view = memory.range(static_cast<std::uint32_t>(c.i32(0)), static_cast<std::uint32_t>(count) * 4U);
    if (!view) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    if (count > 0) {
        std::memcpy(view->data(), handles.data(), view->size());
    }
    c.ret(rec.done(count, list));
}

void playerName(void* u, Call& c) {
    Player* player = nullptr;
    Rec rec("player_name");
    rec.i(c.i32(0)).cap(c.i32(2));
    const std::int32_t status = checkPlayer(worldOf(u), c.i32(0), player);
    c.ret(status != kOk ? rec.done(status) : rec.doneBuffer(c.memory(), c.i32(1), c.i32(2), player->name));
}

void dataGetI64(void* u, Call& c) {
    World& world = worldOf(u);
    const auto key = c.memory().string(c.i32(1), c.i32(2));
    const std::int64_t fallback = c.i64(3);
    Player* player = nullptr;
    std::int64_t result = fallback;
    if (key && checkPlayer(world, c.i32(0), player) == kOk) {
        if (auto* entries = dataOf(world, player)) {
            const auto found = entries->find(std::string{*key});
            if (found != entries->end()) {
                if (const auto* value = std::get_if<std::int64_t>(&found->second)) {
                    result = *value;
                }
            }
        }
    }
    c.ret(Rec("data_get_i64").i(c.i32(0)).str(key).i(fallback).done(result));
}

void dataSetI64(void* u, Call& c) {
    World& world = worldOf(u);
    const auto key = c.memory().string(c.i32(1), c.i32(2));
    Player* player = nullptr;
    std::int32_t status = key ? checkPlayer(world, c.i32(0), player) : kErrInvalid;
    if (status == kOk) {
        auto* entries = dataOf(world, player);
        status = entries == nullptr ? kErrDenied : kOk;
        if (entries != nullptr) {
            (*entries)[std::string{*key}] = c.i64(3);
        }
    }
    c.ret(Rec("data_set_i64").i(c.i32(0)).str(key).i(c.i64(3)).done(status));
}

void dataGetBool(void* u, Call& c) {
    World& world = worldOf(u);
    const auto key = c.memory().string(c.i32(1), c.i32(2));
    const std::int32_t fallback = c.i32(3);
    Player* player = nullptr;
    std::int32_t result = fallback;
    if (key && checkPlayer(world, c.i32(0), player) == kOk) {
        if (auto* entries = dataOf(world, player)) {
            const auto found = entries->find(std::string{*key});
            if (found != entries->end()) {
                if (const auto* value = std::get_if<bool>(&found->second)) {
                    result = *value ? 1 : 0;
                }
            }
        }
    }
    c.ret(Rec("data_get_bool").i(c.i32(0)).str(key).i(fallback).done(result));
}

void dataSetBool(void* u, Call& c) {
    World& world = worldOf(u);
    const auto key = c.memory().string(c.i32(1), c.i32(2));
    Player* player = nullptr;
    std::int32_t status = key ? checkPlayer(world, c.i32(0), player) : kErrInvalid;
    if (status == kOk) {
        auto* entries = dataOf(world, player);
        status = entries == nullptr ? kErrDenied : kOk;
        if (entries != nullptr) {
            (*entries)[std::string{*key}] = c.i32(3) != 0;
        }
    }
    c.ret(Rec("data_set_bool").i(c.i32(0)).str(key).i(c.i32(3)).done(status));
}

void dataGetStr(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto key = memory.string(c.i32(1), c.i32(2));
    Rec rec("data_get_str");
    rec.i(c.i32(0)).str(key).cap(c.i32(4));
    Player* player = nullptr;
    std::int32_t status = key ? checkPlayer(world, c.i32(0), player) : kErrInvalid;
    if (status == kOk && !world.playersDataGranted) {
        status = kErrDenied;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    auto& entries = world.data[player->name];
    const auto found = entries.find(std::string{*key});
    const std::string* value = found != entries.end() ? std::get_if<std::string>(&found->second) : nullptr;
    c.ret(value == nullptr ? rec.done(kErrAbsent) : rec.doneBuffer(memory, c.i32(3), c.i32(4), *value));
}

void dataSetStr(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto key = memory.string(c.i32(1), c.i32(2));
    const auto value = memory.string(c.i32(3), c.i32(4));
    Player* player = nullptr;
    std::int32_t status = key && value ? checkPlayer(world, c.i32(0), player) : kErrInvalid;
    if (status == kOk) {
        auto* entries = dataOf(world, player);
        status = entries == nullptr ? kErrDenied : kOk;
        if (entries != nullptr) {
            (*entries)[std::string{*key}] = std::string{*value};
        }
    }
    c.ret(Rec("data_set_str").i(c.i32(0)).str(key).str(value).done(status));
}

void dataRemove(void* u, Call& c) {
    World& world = worldOf(u);
    const auto key = c.memory().string(c.i32(1), c.i32(2));
    Player* player = nullptr;
    std::int32_t status = key ? checkPlayer(world, c.i32(0), player) : kErrInvalid;
    if (status == kOk) {
        auto* entries = dataOf(world, player);
        status = entries == nullptr ? kErrDenied : (entries->erase(std::string{*key}) > 0 ? kOk : kErrAbsent);
    }
    c.ret(Rec("data_remove").i(c.i32(0)).str(key).done(status));
}

void storeTop(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto key = memory.string(c.i32(0), c.i32(1));
    Rec rec("store_top");
    rec.str(key).i(c.i32(2)).cap(c.i32(4));
    if (!world.playersDataGranted) {
        c.ret(rec.done(kErrDenied));
        return;
    }
    if (!key || c.i32(2) <= 0) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    std::vector<std::pair<std::string, std::int64_t>> rows;
    for (const auto& [name, entries] : world.data) {
        const auto found = entries.find(std::string{*key});
        if (found != entries.end()) {
            if (const auto* value = std::get_if<std::int64_t>(&found->second)) {
                rows.emplace_back(name, *value);
            }
        }
    }
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    rows.resize(std::min(rows.size(), static_cast<std::size_t>(c.i32(2))));
    std::string packed;
    std::string shown = "[";
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const auto length = static_cast<std::uint32_t>(rows[index].first.size());
        char raw[8];
        std::memcpy(raw, &length, 4);
        packed.append(raw, 4);
        packed += rows[index].first;
        std::memcpy(raw, &rows[index].second, 8);
        packed.append(raw, 8);
        shown += (index == 0 ? "" : ", ") + quote(rows[index].first) + "=" + std::to_string(rows[index].second);
    }
    shown += "]";
    const std::int32_t result = memory.put(c.i32(3), c.i32(4), packed);
    const bool written = result >= 0 && static_cast<std::size_t>(result) == packed.size() && result <= c.i32(4);
    c.ret(written ? rec.done(result, shown) : rec.done(result));
}

void placeholderRegister(void* u, Call& c) {
    World& world = worldOf(u);
    const auto name = c.memory().string(c.i32(0), c.i32(1));
    Rec rec("placeholder_register");
    rec.str(name).i(c.i32(2));
    const bool duplicate = name && std::any_of(world.placeholders.begin(), world.placeholders.end(),
                                               [&](const RegisteredPlaceholder& entry) { return entry.name == *name; });
    if (!name || name->empty() || duplicate) {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    world.placeholders.push_back(RegisteredPlaceholder{std::string{*name}, c.i32(2)});
    c.ret(rec.done(kOk));
}

void placeholderPlayer(void* u, Call& c) {
    Context* context = nullptr;
    const std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Placeholder, context);
    c.ret(Rec("placeholder_player").i(c.i32(0)).done(status != kOk ? status : context->player));
}

void placeholderResult(void* u, Call& c) {
    const auto text = c.memory().string(c.i32(1), c.i32(2));
    Context* context = nullptr;
    std::int32_t status = worldOf(u).checkContext(c.i32(0), ContextKind::Placeholder, context);
    if (status == kOk) {
        if (text) {
            context->result = std::string{*text};
        } else {
            status = kErrInvalid;
        }
    }
    c.ret(Rec("placeholder_result").i(c.i32(0)).str(text).done(status));
}
} // namespace

void addDataImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"player_by_name", playerByName, user},
                              {"players_online", playersOnline, user},
                              {"player_name", playerName, user},
                              {"data_get_i64", dataGetI64, user},
                              {"data_set_i64", dataSetI64, user},
                              {"data_get_bool", dataGetBool, user},
                              {"data_set_bool", dataSetBool, user},
                              {"data_get_str", dataGetStr, user},
                              {"data_set_str", dataSetStr, user},
                              {"data_remove", dataRemove, user},
                              {"store_top", storeTop, user},
                              {"placeholder_register", placeholderRegister, user},
                              {"placeholder_player", placeholderPlayer, user},
                              {"placeholder_result", placeholderResult, user},
                          });
}

} // namespace basalt::wire::wasm::trace
