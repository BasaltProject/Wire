#include "HostHud.hpp"

#include <algorithm>
#include <array>

namespace basalt::wire::wasm::trace {
namespace hud {
namespace {
constexpr std::array<std::string_view, 16> kTeamColors{
    "black",     "dark_blue", "dark_green", "dark_aqua", "dark_red", "dark_purple",  "gold",   "gray",
    "dark_gray", "blue",      "green",      "aqua",      "red",      "light_purple", "yellow", "white"};
constexpr std::array<std::string_view, 8> kCriteria{"dummy", "trigger", "health", "level",
                                                    "food",  "air",     "armor",  "xp"};

[[nodiscard]] bool validSlot(std::string_view name) {
    if (name == "list" || name == "sidebar" || name == "below_name") {
        return true;
    }
    constexpr std::string_view kTeamPrefix = "sidebar.team.";
    return name.starts_with(kTeamPrefix) &&
           std::ranges::find(kTeamColors, name.substr(kTeamPrefix.size())) != kTeamColors.end();
}

template <typename T>
[[nodiscard]] std::int32_t findIn(std::vector<T>& list, std::int32_t handle, T*& out) {
    if (handle < 1 || static_cast<std::size_t>(handle) > list.size()) {
        return kErrInvalid;
    }
    T& item = list[static_cast<std::size_t>(handle - 1)];
    if (!item.alive) {
        return kErrStale;
    }
    out = &item;
    return kOk;
}

[[nodiscard]] std::int32_t boardOf(World& world, std::int32_t handle, Board*& out) {
    return findIn(stateOf(world).boards, handle, out);
}

[[nodiscard]] Team* teamNamed(Board& board, std::string_view name) {
    const auto found = std::ranges::find_if(board.teams, [&](const Team& team) { return team.name == name; });
    return found == board.teams.end() ? nullptr : &*found;
}

void scoreboardMain(void* u, Call& c) {
    State& state = stateOf(worldOf(u));
    if (state.mainHandle == 0) {
        state.boards.push_back(Board{.main = true});
        state.mainHandle = static_cast<std::int32_t>(state.boards.size());
    }
    c.ret(Rec("hud_scoreboard_main").done(state.mainHandle));
}

void scoreboardCreate(void* u, Call& c) {
    State& state = stateOf(worldOf(u));
    state.boards.push_back(Board{});
    c.ret(Rec("hud_scoreboard_create").done(static_cast<std::int32_t>(state.boards.size())));
}

void scoreboardRelease(void* u, Call& c) {
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk) {
        if (board->main) {
            status = kErrInvalid;
        } else {
            board->alive = false;
        }
    }
    c.ret(Rec("hud_scoreboard_release").i(c.i32(0)).done(status));
}

void scoreboardList(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto what = memory.string(c.i32(1), c.i32(2));
    Rec rec("hud_scoreboard_list");
    rec.i(c.i32(0)).str(what).cap(c.i32(4));
    Board* board = nullptr;
    const std::int32_t status = boardOf(world, c.i32(0), board);
    if (status != kOk || !what) {
        c.ret(rec.done(status != kOk ? status : kErrInvalid));
        return;
    }
    std::vector<std::string> names;
    if (*what == "objectives") {
        names = board->objectives;
    } else if (*what == "entries") {
        for (const auto& [entry, scores] : board->scores) {
            if (!scores.empty()) {
                names.push_back(entry);
            }
        }
    } else if (*what == "teams") {
        for (const Team& team : board->teams) {
            names.push_back(team.name);
        }
    } else {
        c.ret(rec.done(kErrInvalid));
        return;
    }
    c.ret(doneValue(rec, memory, c.i32(3), c.i32(4), stringList(names)));
}

void objectiveAdd(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    const std::expected<Value, std::int32_t> spec = readSpec(memory, c.i32(3), c.i32(4));
    Rec rec("hud_objective_add");
    rec.i(c.i32(0)).str(name).value(spec);
    Board* board = nullptr;
    std::int32_t status = boardOf(world, c.i32(0), board);
    if (status == kOk && (!name || name->empty() || !spec)) {
        status = kErrInvalid;
    }
    std::optional<std::string> criterion;
    std::optional<std::string> title;
    std::optional<bool> hearts;
    if (status == kOk) {
        status = takeString(*spec, "criterion", criterion);
        status = status != kOk ? status : takeText(world, *spec, "title", title);
        status = status != kOk ? status : takeBool(*spec, "hearts", hearts);
    }
    if (status == kOk) {
        const bool known = std::ranges::find(kCriteria, criterion.value_or("dummy")) != kCriteria.end();
        const bool taken = std::ranges::find(board->objectives, std::string{*name}) != board->objectives.end();
        if (!known || taken) {
            status = kErrInvalid;
        } else {
            board->objectives.emplace_back(*name);
        }
    }
    c.ret(rec.done(status));
}

void objectiveRemove(void* u, Call& c) {
    const auto name = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("hud_objective_remove");
    rec.i(c.i32(0)).str(name);
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk && !name) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        const auto found = std::ranges::find(board->objectives, std::string{*name});
        if (found == board->objectives.end()) {
            status = kErrAbsent;
        } else {
            board->objectives.erase(found);
            for (auto& [entry, scores] : board->scores) {
                scores.erase(std::string{*name});
            }
            std::erase_if(board->display, [&](const auto& slot) { return slot.second == *name; });
        }
    }
    c.ret(rec.done(status));
}

void displaySet(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto slot = memory.string(c.i32(1), c.i32(2));
    const auto objective = memory.string(c.i32(3), c.i32(4));
    Rec rec("hud_display_set");
    rec.i(c.i32(0)).str(slot).str(objective);
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk && (!slot || !objective || !validSlot(*slot))) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        if (objective->empty()) {
            board->display.erase(std::string{*slot});
        } else if (std::ranges::find(board->objectives, std::string{*objective}) == board->objectives.end()) {
            status = kErrAbsent;
        } else {
            board->display[std::string{*slot}] = std::string{*objective};
        }
    }
    c.ret(rec.done(status));
}

void displayGet(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto slot = memory.string(c.i32(1), c.i32(2));
    Rec rec("hud_display_get");
    rec.i(c.i32(0)).str(slot).cap(c.i32(4));
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk && (!slot || !validSlot(*slot))) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const auto shown = board->display.find(std::string{*slot});
    if (shown == board->display.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    c.ret(rec.doneBuffer(memory, c.i32(3), c.i32(4), shown->second));
}

void scoreGet(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto entry = memory.string(c.i32(1), c.i32(2));
    const auto objective = memory.string(c.i32(3), c.i32(4));
    Rec rec("hud_score_get");
    rec.i(c.i32(0)).str(entry).str(objective);
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk && (!entry || !objective)) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    const auto scores = board->scores.find(std::string{*entry});
    const auto found = scores == board->scores.end() ? std::map<std::string, std::int32_t>::iterator{}
                                                     : scores->second.find(std::string{*objective});
    if (scores == board->scores.end() || found == scores->second.end()) {
        c.ret(rec.done(kErrAbsent));
        return;
    }
    const bool wrote = memory.write(c.i32(5), found->second);
    c.ret(wrote ? rec.done(kOk, "out=" + std::to_string(found->second)) : rec.done(kErrInvalid));
}

void scoreSet(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto entry = memory.string(c.i32(1), c.i32(2));
    const auto objective = memory.string(c.i32(3), c.i32(4));
    Rec rec("hud_score_set");
    rec.i(c.i32(0)).str(entry).str(objective).i(c.i32(5));
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk && (!entry || !objective || entry->empty())) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        if (std::ranges::find(board->objectives, std::string{*objective}) == board->objectives.end()) {
            status = kErrInvalid;
        } else {
            board->scores[std::string{*entry}][std::string{*objective}] = c.i32(5);
        }
    }
    c.ret(rec.done(status));
}

void scoreReset(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto entry = memory.string(c.i32(1), c.i32(2));
    const auto objective = memory.string(c.i32(3), c.i32(4));
    Rec rec("hud_score_reset");
    rec.i(c.i32(0)).str(entry).str(objective);
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk && (!entry || !objective)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        if (objective->empty()) {
            board->scores.erase(std::string{*entry});
        } else if (const auto scores = board->scores.find(std::string{*entry}); scores != board->scores.end()) {
            scores->second.erase(std::string{*objective});
        }
    }
    c.ret(rec.done(status));
}

void teamAdd(void* u, Call& c) {
    const auto name = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("hud_team_add");
    rec.i(c.i32(0)).str(name);
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk && (!name || name->empty() || teamNamed(*board, *name) != nullptr)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        board->teams.push_back(Team{.name = std::string{*name}});
    }
    c.ret(rec.done(status));
}

[[nodiscard]] std::int32_t applyTeamSpec(World& world, const Value& spec, Team& team) {
    std::int32_t status = kOk;
    for (const std::string_view key : {"display_name", "prefix", "suffix"}) {
        std::optional<std::string> text;
        status = status != kOk ? status : takeText(world, spec, key, text);
        if (text) {
            team.options[std::string{key}] = Value{*text};
        }
    }
    for (const std::string_view key : {"color", "name_tags", "death_messages", "collision"}) {
        std::optional<std::string> word;
        status = status != kOk ? status : takeString(spec, key, word);
        if (word) {
            team.options[std::string{key}] = Value{*word};
        }
    }
    for (const std::string_view key : {"friendly_fire", "see_invisible"}) {
        std::optional<bool> flag;
        status = status != kOk ? status : takeBool(spec, key, flag);
        if (flag) {
            team.options[std::string{key}] = Value{*flag};
        }
    }
    return status;
}

void teamSet(void* u, Call& c) {
    World& world = worldOf(u);
    const GuestMemory memory = c.memory();
    const auto name = memory.string(c.i32(1), c.i32(2));
    const std::expected<Value, std::int32_t> spec = readSpec(memory, c.i32(3), c.i32(4));
    Rec rec("hud_team_set");
    rec.i(c.i32(0)).str(name).value(spec);
    Board* board = nullptr;
    std::int32_t status = boardOf(world, c.i32(0), board);
    if (status == kOk && (!name || !spec)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        Team* team = teamNamed(*board, *name);
        if (team == nullptr) {
            status = kErrAbsent;
        } else {
            Team staged = *team;
            status = applyTeamSpec(world, *spec, staged);
            if (status == kErrAbsent) {
                status = kErrInvalid;
            }
            if (status == kOk) {
                *team = std::move(staged);
            }
        }
    }
    c.ret(rec.done(status));
}

void teamRemove(void* u, Call& c) {
    const auto name = c.memory().string(c.i32(1), c.i32(2));
    Rec rec("hud_team_remove");
    rec.i(c.i32(0)).str(name);
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk && !name) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        const std::size_t before = board->teams.size();
        std::erase_if(board->teams, [&](const Team& team) { return team.name == *name; });
        status = board->teams.size() == before ? kErrAbsent : kOk;
    }
    c.ret(rec.done(status));
}

void teamJoin(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto team = memory.string(c.i32(1), c.i32(2));
    const auto entry = memory.string(c.i32(3), c.i32(4));
    Rec rec("hud_team_join");
    rec.i(c.i32(0)).str(team).str(entry);
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk && (!team || !entry || entry->empty())) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        Team* joined = teamNamed(*board, *team);
        if (joined == nullptr) {
            status = kErrAbsent;
        } else {
            for (Team& other : board->teams) {
                std::erase(other.members, std::string{*entry});
            }
            joined->members.emplace_back(*entry);
        }
    }
    c.ret(rec.done(status));
}

void teamLeave(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto team = memory.string(c.i32(1), c.i32(2));
    const auto entry = memory.string(c.i32(3), c.i32(4));
    Rec rec("hud_team_leave");
    rec.i(c.i32(0)).str(team).str(entry);
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk && (!team || !entry)) {
        status = kErrInvalid;
    }
    if (status == kOk) {
        Team* left = teamNamed(*board, *team);
        const bool member = left != nullptr && std::erase(left->members, std::string{*entry}) > 0;
        status = member ? kOk : kErrAbsent;
    }
    c.ret(rec.done(status));
}

void teamOf(void* u, Call& c) {
    const GuestMemory memory = c.memory();
    const auto entry = memory.string(c.i32(1), c.i32(2));
    Rec rec("hud_team_of");
    rec.i(c.i32(0)).str(entry).cap(c.i32(4));
    Board* board = nullptr;
    std::int32_t status = boardOf(worldOf(u), c.i32(0), board);
    if (status == kOk && !entry) {
        status = kErrInvalid;
    }
    if (status != kOk) {
        c.ret(rec.done(status));
        return;
    }
    for (const Team& team : board->teams) {
        if (std::ranges::find(team.members, std::string{*entry}) != team.members.end()) {
            c.ret(rec.doneBuffer(memory, c.i32(3), c.i32(4), team.name));
            return;
        }
    }
    c.ret(rec.done(kErrAbsent));
}

void scoreboardShow(void* u, Call& c) {
    World& world = worldOf(u);
    Player* player = nullptr;
    std::int32_t status = checkPlayer(world, c.i32(0), player);
    Board* board = nullptr;
    if (status == kOk) {
        status = boardOf(world, c.i32(1), board);
    }
    if (status == kOk) {
        stateOf(world).shown[player->name] = c.i32(1);
    }
    c.ret(Rec("hud_scoreboard_show").i(c.i32(0)).i(c.i32(1)).done(status));
}

void scoreboardShown(void* u, Call& c) {
    World& world = worldOf(u);
    State& state = stateOf(world);
    Player* player = nullptr;
    const std::int32_t status = checkPlayer(world, c.i32(0), player);
    if (status != kOk) {
        c.ret(Rec("hud_scoreboard_shown").i(c.i32(0)).done(status));
        return;
    }
    const auto shown = state.shown.find(player->name);
    if (shown == state.shown.end() && state.mainHandle == 0) {
        state.boards.push_back(Board{.main = true});
        state.mainHandle = static_cast<std::int32_t>(state.boards.size());
    }
    c.ret(Rec("hud_scoreboard_shown").i(c.i32(0)).done(shown == state.shown.end() ? state.mainHandle : shown->second));
}
} // namespace
} // namespace hud

void addHudImports(World& world, std::vector<HostImport>& out) {
    World* user = &world;
    out.insert(out.end(), {
                              {"hud_scoreboard_main", hud::scoreboardMain, user},
                              {"hud_scoreboard_create", hud::scoreboardCreate, user},
                              {"hud_scoreboard_release", hud::scoreboardRelease, user},
                              {"hud_scoreboard_list", hud::scoreboardList, user},
                              {"hud_objective_add", hud::objectiveAdd, user},
                              {"hud_objective_remove", hud::objectiveRemove, user},
                              {"hud_display_set", hud::displaySet, user},
                              {"hud_display_get", hud::displayGet, user},
                              {"hud_score_get", hud::scoreGet, user},
                              {"hud_score_set", hud::scoreSet, user},
                              {"hud_score_reset", hud::scoreReset, user},
                              {"hud_team_add", hud::teamAdd, user},
                              {"hud_team_set", hud::teamSet, user},
                              {"hud_team_remove", hud::teamRemove, user},
                              {"hud_team_join", hud::teamJoin, user},
                              {"hud_team_leave", hud::teamLeave, user},
                              {"hud_team_of", hud::teamOf, user},
                              {"hud_scoreboard_show", hud::scoreboardShow, user},
                              {"hud_scoreboard_shown", hud::scoreboardShown, user},
                          });
}

} // namespace basalt::wire::wasm::trace
