#pragma once

#include "HostApi.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::wasm::trace::items {

struct Item {
    std::string id;
    std::int64_t count = 1;
    std::optional<std::string> name;
    std::vector<std::string> lore;
    std::int64_t damage = 0;
    std::map<std::string, std::int64_t> enchantments;
    std::map<std::string, std::string> components;

    [[nodiscard]] bool sameKind(const Item& other) const;
};

using Slot = std::optional<Item>;

[[nodiscard]] std::optional<Item> parseItem(const Value& value);
[[nodiscard]] Value itemValue(const Slot& item);
[[nodiscard]] std::int64_t maxStack(const Item& item);

/// Null is an empty slot (nullopt inside); an unreadable or bad value is the error result.
[[nodiscard]] std::expected<Slot, std::int32_t> readSlotValue(const GuestMemory& memory, std::int32_t ptr,
                                                              std::int32_t len);

enum class Kind : std::uint8_t { Player, Block, Window, Ender, Entity };

struct Inventory {
    Kind kind = Kind::Player;
    std::string owner;
    std::int32_t container = 0;
    bool released = false;
};

struct Window {
    std::int32_t container = 0;
    std::string kind;
    bool plugin = false;
    std::string title;
    std::vector<Slot> slots;
    Slot cursor;
    std::map<std::string, std::int64_t> properties;
    std::int32_t menu = 0;
};

struct MenuSlotDef {
    Slot item;
    std::int32_t token = 0;
};

struct MenuDef {
    std::string title;
    std::int64_t rows = 3;
    std::string kind;
    std::map<std::int32_t, MenuSlotDef> slots;
};

struct Merchant {
    std::string title;
    std::size_t offers = 0;
};

struct State {
    std::vector<Inventory> handles;
    std::map<std::string, std::vector<Slot>> players;
    std::map<std::string, std::int32_t> held;
    std::map<std::string, std::vector<Slot>> ender;
    std::map<std::string, std::vector<Slot>> blocks;
    std::map<std::string, Window> windows;
    std::map<std::string, std::map<std::string, std::int64_t>> cooldowns;
    std::map<std::string, std::vector<std::string>> book;
    std::vector<std::string> recipes;
    std::map<std::string, std::string> recipeTypes;
    std::vector<MenuDef> menus;
    std::int32_t nextContainer = 1;
    bool seeded = false;
};

[[nodiscard]] State& stateOf(World& world);

/// Returns the guest-visible result and records the line; the value is traced when it fitted in `cap`.
std::int32_t doneValue(Rec& rec, const GuestMemory& memory, std::int32_t ptr, std::int32_t cap, const Value& value);

void seedState(World& world);

/// Opens `window` for the player, replacing one they had, and records the effect.
void openWindowFor(World& world, Player& player, Window window);

} // namespace basalt::wire::wasm::trace::items
