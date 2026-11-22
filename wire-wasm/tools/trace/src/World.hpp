#pragma once

#include <basalt/wire/common/toml/Toml.hpp>
#include <basalt/wire/plugins/CommandArguments.hpp>
#include <basalt/wire/plugins/CommandSpec.hpp>
#include <basalt/wire/plugins/PluginManifest.hpp>
#include <basalt/wire/wasm/Instance.hpp>
#include <basalt/wire/wasm/Value.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::wasm::trace {

enum class ContextKind : std::uint8_t { Event, Command, Task, Placeholder, MenuClick, Completion };

struct Player {
    std::int32_t handle = 0;
    std::string name;
    bool online = false;
};

using DataValue = std::variant<std::int64_t, bool, std::string>;

struct Context {
    std::int32_t id = 0;
    ContextKind kind = ContextKind::Event;
    bool live = false;
    std::int32_t player = 0;
    std::string eventName;
    std::map<std::string, std::variant<std::string, std::int64_t>> fields;
    bool cancelled = false;
    plugins::CommandSpec spec;
    std::vector<plugins::CommandValue> values;
    std::string source;
    bool console = false;
    std::optional<std::string> result;
    std::optional<Value> value;
};

struct Task {
    std::int32_t handle = 0;
    std::int32_t token = 0;
    std::int64_t due = 0;
    std::int64_t period = 0;
    bool alive = true;
};

struct Listener {
    std::string event;
    std::int32_t priority = 0;
    std::int32_t token = 0;
};

struct RegisteredCommand {
    plugins::CommandSpec spec;
    std::int32_t token = 0;
};

struct RegisteredPlaceholder {
    std::string name;
    std::int32_t token = 0;
};

struct MenuSlot {
    std::string item;
    std::string name;
    std::string lore;
    std::int32_t token = 0;
};

struct Menu {
    std::string title;
    std::int32_t rows = 0;
    std::map<std::int32_t, MenuSlot> slots;
};

class World {
public:
    common::toml::TomlTable config;
    common::toml::TomlTable lang;
    bool playersDataGranted = false;
    plugins::PluginCapabilities capabilities;
    std::vector<std::string> exportedServices;

    Instance* instance = nullptr;
    bool disabled = false;
    std::int64_t now = 0;

    std::vector<Player> players;
    std::map<std::string, std::map<std::string, DataValue>> data;
    std::vector<Listener> listeners;
    std::vector<RegisteredCommand> commands;
    std::vector<RegisteredPlaceholder> placeholders;
    std::vector<Menu> menus;
    std::vector<Task> tasks;
    Context current;
    std::map<std::string, std::shared_ptr<void>> areaStates;
    /// Host work that runs as soon as the current scenario step is done (the owner being free again).
    std::vector<std::function<void()>> deferred;
    /// Completions an area answers later, delivered at the end of the next tick.
    std::vector<std::function<void()>> tickEnd;
    /// Host timers by due tick, run at the start of the tick before plugin tasks.
    std::vector<std::pair<std::int64_t, std::function<void()>>> timers;

    /// The state an area's trace host keeps between calls, created on first use (the area name is the key).
    template <typename T>
    [[nodiscard]] T& areaState(const std::string& area) {
        std::shared_ptr<void>& slot = areaStates[area];
        if (!slot) {
            slot = std::make_shared<T>();
        }
        return *static_cast<T*>(slot.get());
    }

    [[nodiscard]] Player* playerByHandle(std::int32_t handle);
    [[nodiscard]] Player* playerByName(std::string_view name);
    [[nodiscard]] std::int32_t checkContext(std::int32_t id, ContextKind kind, Context*& out);

    void join(std::string_view name);
    void leave(std::string_view name);
    void runCommand(std::string_view source, std::string_view line);
    void advance(std::int64_t ticks);
    void click(std::int32_t menuIndex, std::int32_t slot, std::string_view playerName);
    void placeholder(std::string_view name, std::string_view playerName);
    void drain();
    void enable();
    void disable();

    void fireEvent(std::string_view name, std::int32_t player,
                   std::map<std::string, std::variant<std::string, std::int64_t>> fields);
    [[nodiscard]] std::int32_t invoke(std::int32_t token, Context context);

private:
    std::int32_t nextHandle_ = 1;
    std::int32_t nextContext_ = 1;
};

void emitLine(std::string_view line);

} // namespace basalt::wire::wasm::trace
