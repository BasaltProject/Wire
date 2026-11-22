#include "World.hpp"

#include "HostValues.hpp"
#include "Text.hpp"

#include <basalt/wire/wasm/Abi.hpp>

#include <algorithm>
#include <cstdio>

namespace basalt::wire::wasm::trace {
namespace {
[[nodiscard]] std::vector<std::string> splitWords(std::string_view line) {
    std::vector<std::string> words;
    std::size_t at = 0;
    while (at < line.size()) {
        while (at < line.size() && line[at] == ' ') {
            ++at;
        }
        const std::size_t start = at;
        while (at < line.size() && line[at] != ' ') {
            ++at;
        }
        if (at > start) {
            words.emplace_back(line.substr(start, at - start));
        }
    }
    return words;
}

[[nodiscard]] std::int64_t onlineCount(const World& world) {
    return std::count_if(world.players.begin(), world.players.end(), [](const Player& p) { return p.online; });
}

[[nodiscard]] std::string_view errorName(ErrorCode code) {
    switch (code) {
    case ErrorCode::Trap:
        return "trap";
    case ErrorCode::Interrupted:
        return "interrupted";
    default:
        return "error";
    }
}
} // namespace

void emitLine(std::string_view line) {
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fputc('\n', stdout);
}

Player* World::playerByHandle(std::int32_t handle) {
    for (Player& player : players) {
        if (player.handle == handle) {
            return &player;
        }
    }
    return nullptr;
}

Player* World::playerByName(std::string_view name) {
    for (Player& player : players) {
        if (player.online && player.name == name) {
            return &player;
        }
    }
    return nullptr;
}

std::int32_t World::checkContext(std::int32_t id, ContextKind kind, Context*& out) {
    if (id == current.id && current.live) {
        if (current.kind != kind) {
            return kErrInvalid;
        }
        out = &current;
        return kOk;
    }
    return id >= 1 && id < nextContext_ ? kErrStale : kErrInvalid;
}

std::int32_t World::invoke(std::int32_t token, Context context) {
    if (disabled) {
        emitLine("world skipped token=" + std::to_string(token) + " (plugin disabled)");
        return kErrDenied;
    }
    context.id = nextContext_++;
    context.live = true;
    current = std::move(context);
    emitLine("callback token=" + std::to_string(token) + " ctx=" + std::to_string(current.id));
    const Result<void> result = instance->callback(token, current.id);
    current.live = false;
    if (!result) {
        disabled = true;
        emitLine("world plugin-disabled " + std::string{errorName(result.error().code)});
        return kErrDenied;
    }
    return kOk;
}

void World::fireEvent(std::string_view name, std::int32_t player,
                      std::map<std::string, std::variant<std::string, std::int64_t>> fields) {
    std::vector<Listener> matching;
    for (const Listener& listener : listeners) {
        if (listener.event == name) {
            matching.push_back(listener);
        }
    }
    std::stable_sort(matching.begin(), matching.end(),
                     [](const Listener& a, const Listener& b) { return a.priority < b.priority; });
    bool cancelled = false;
    for (const Listener& listener : matching) {
        Context context;
        context.kind = ContextKind::Event;
        context.player = player;
        context.eventName = std::string{name};
        context.fields = fields;
        context.cancelled = cancelled;
        (void)invoke(listener.token, std::move(context));
        cancelled = current.cancelled;
    }
    emitLine("world event " + std::string{name} + " listeners=" + std::to_string(matching.size()) +
             " cancelled=" + (cancelled ? "1" : "0"));
}

void World::enable() {
    emitLine("init");
    if (const Result<void> result = instance->init(); !result) {
        disabled = true;
        emitLine("world plugin-disabled " + std::string{errorName(result.error().code)});
        return;
    }
    fireEvent("plugin.enable", 0, {});
}

void World::disable() {
    fireEvent("plugin.disable", 0, {});
    emitLine("world plugin-disabled requested");
    disabled = true;
}

void World::join(std::string_view name) {
    if (playerByName(name) != nullptr) {
        emitLine("world join-ignored " + std::string{name} + " (already online)");
        return;
    }
    Player player;
    player.handle = nextHandle_++;
    player.name = std::string{name};
    player.online = true;
    players.push_back(player);
    data.try_emplace(player.name);
    emitLine("world join " + player.name + " handle=" + std::to_string(player.handle));
    fireEvent("player.join", player.handle, {{"name", player.name}, {"online", onlineCount(*this)}});
}

void World::leave(std::string_view name) {
    Player* player = playerByName(name);
    if (player == nullptr) {
        emitLine("world leave-ignored " + std::string{name} + " (not online)");
        return;
    }
    const std::int32_t handle = player->handle;
    fireEvent("player.leave", handle, {{"name", player->name}, {"online", onlineCount(*this) - 1}});
    if (Player* again = playerByHandle(handle)) {
        again->online = false;
    }
    emitLine("world leave " + std::string{name});
}

void World::runCommand(std::string_view source, std::string_view line) {
    const bool console = source == "console";
    std::int32_t player = 0;
    if (!console) {
        Player* found = playerByName(source);
        if (found == nullptr) {
            emitLine("world command-rejected " + std::string{source} + " (not online)");
            return;
        }
        player = found->handle;
    }
    if (line.starts_with('/')) {
        line.remove_prefix(1);
    }
    const std::vector<std::string> words = splitWords(line);
    const RegisteredCommand* best = nullptr;
    for (const RegisteredCommand& command : commands) {
        const auto& literals = command.spec.literals;
        if (words.size() < literals.size() || !std::equal(literals.begin(), literals.end(), words.begin())) {
            continue;
        }
        if (best == nullptr || literals.size() > best->spec.literals.size()) {
            best = &command;
        }
    }
    if (best == nullptr) {
        emitLine("world unknown-command " + quote(line));
        return;
    }
    const std::optional<std::vector<plugins::CommandValue>> values = plugins::parseCommandWords(best->spec, words);
    if (!values) {
        emitLine("world usage " + std::string{source} + " " + quote(best->spec.usage()));
        return;
    }
    Context context;
    context.kind = ContextKind::Command;
    context.player = player;
    context.spec = best->spec;
    context.values = *values;
    context.source = console ? "CONSOLE" : std::string{source};
    context.console = console;
    (void)invoke(best->token, std::move(context));
}

void World::drain() {
    while (!deferred.empty()) {
        std::function<void()> next = std::move(deferred.front());
        deferred.erase(deferred.begin());
        next();
    }
}

void World::advance(std::int64_t ticks) {
    for (std::int64_t step = 0; step < ticks; ++step) {
        ++now;
        for (std::size_t at = 0; at < timers.size();) {
            if (timers[at].first <= now) {
                std::function<void()> fire = std::move(timers[at].second);
                timers.erase(timers.begin() + static_cast<std::ptrdiff_t>(at));
                fire();
            } else {
                ++at;
            }
        }
        for (;;) {
            Task* next = nullptr;
            for (Task& task : tasks) {
                if (task.alive && task.due <= now && (next == nullptr || task.due < next->due)) {
                    next = &task;
                }
            }
            if (next == nullptr) {
                break;
            }
            const std::int32_t handle = next->handle;
            const std::int32_t token = next->token;
            if (next->period > 0) {
                next->due = now + next->period;
            } else {
                next->alive = false;
            }
            emitLine("world task " + std::to_string(handle) + " fires at=" + std::to_string(now));
            Context context;
            context.kind = ContextKind::Task;
            (void)invoke(token, std::move(context));
        }
        for (std::size_t at = 0; at < tickEnd.size(); ++at) {
            std::function<void()> next = std::move(tickEnd[at]);
            next();
        }
        tickEnd.clear();
        drain();
    }
}

void World::click(std::int32_t menuIndex, std::int32_t slot, std::string_view playerName) {
    Player* player = playerByName(playerName);
    if (player == nullptr) {
        emitLine("world click-rejected " + std::string{playerName} + " (not online)");
        return;
    }
    if (menuIndex < 1 || static_cast<std::size_t>(menuIndex) > menus.size()) {
        emitLine("world click-rejected menu=" + std::to_string(menuIndex) + " (no such menu)");
        return;
    }
    const Menu& menu = menus[static_cast<std::size_t>(menuIndex - 1)];
    const auto found = menu.slots.find(slot);
    if (found == menu.slots.end()) {
        emitLine("world click-ignored menu=" + std::to_string(menuIndex) + " slot=" + std::to_string(slot));
        return;
    }
    Context context;
    context.kind = ContextKind::MenuClick;
    context.player = player->handle;
    (void)invoke(found->second.token, std::move(context));
}

void World::placeholder(std::string_view name, std::string_view playerName) {
    std::int32_t player = 0;
    if (playerName != "none") {
        Player* found = playerByName(playerName);
        if (found == nullptr) {
            emitLine("world placeholder-rejected " + std::string{playerName} + " (not online)");
            return;
        }
        player = found->handle;
    }
    for (const RegisteredPlaceholder& entry : placeholders) {
        if (entry.name != name) {
            continue;
        }
        Context context;
        context.kind = ContextKind::Placeholder;
        context.player = player;
        (void)invoke(entry.token, std::move(context));
        emitLine("world placeholder " + std::string{name} + " = " +
                 (current.result ? quote(*current.result) : std::string{"<none>"}));
        return;
    }
    emitLine("world placeholder-unknown " + std::string{name});
}

} // namespace basalt::wire::wasm::trace
