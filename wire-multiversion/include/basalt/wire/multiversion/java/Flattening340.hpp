#pragma once

#include <basalt/wire/multiversion/IdMap.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

/// What 1.12.2 numbers mean in 1.13: blocks and items by `id << 4 | data`, read from the world upgrade.
namespace basalt::wire::multiversion::java::flattening340 {

struct NamedId {
    std::string_view name;
    std::int32_t id;
};

struct Rename {
    std::string_view from;
    std::string_view to;
};

struct Potted {
    std::string_view item;
    std::int32_t data;
    std::int32_t state;
};

/// A 1.13 block whose state 1.12 left to the client to work out, with its properties as `name=v1,v2;...`.
struct Shape {
    std::string_view name;
    std::int32_t base;
    std::int32_t count;
    std::string_view properties;
};

/// The first 1.13 state of each kind, by skull type or dye colour.
struct Variants {
    std::array<std::int32_t, 6> skulls;
    std::array<std::int32_t, 6> wallSkulls;
    std::array<std::int32_t, 16> beds;
    std::array<std::int32_t, 16> banners;
    std::array<std::int32_t, 16> wallBanners;
};

[[nodiscard]] const IdMap& blockStates();
[[nodiscard]] const IdMap& blockIds();
[[nodiscard]] const IdMap& items();
/// A 1.13 item to the first 1.12 `id << 4 | damage` that names it.
[[nodiscard]] const IdMap& itemsDown();
[[nodiscard]] const IdMap& mobs();
[[nodiscard]] const IdMap& enchantmentIds();
[[nodiscard]] const IdMap& sounds();
[[nodiscard]] std::span<const std::int32_t> particles();
[[nodiscard]] std::span<const std::int32_t> damageable();
[[nodiscard]] std::span<const NamedId> spawnEggs();
[[nodiscard]] std::span<const NamedId> enchantments();
[[nodiscard]] std::span<const Rename> soundNames();
/// 1.12 translation keys the newest client no longer has, to one of its keys with the same English text.
[[nodiscard]] std::span<const Rename> translations();
[[nodiscard]] std::span<const Potted> pots();
[[nodiscard]] const Variants& variants();
/// 1.13 states with a full, solid face on every side, which fences, panes and walls join to.
[[nodiscard]] const IdMap& sturdy();
[[nodiscard]] std::span<const Shape> shapes();
/// Sound ids of 1.10 in 1.11, and of 1.9.4 in 1.10, matched by name.
[[nodiscard]] const IdMap& sounds210();
[[nodiscard]] const IdMap& sounds110();
/// The entity and block entity names before 1.11, to the ids that replaced them.
[[nodiscard]] std::span<const Rename> entityNames();
[[nodiscard]] std::span<const Rename> blockEntityNames();
/// 1.8 sound event names to the 1.9 event that plays the same files.
[[nodiscard]] std::span<const Rename> sounds18();
/// What 1.8 spawn egg damage and potion damage name, by value; empty where nothing.
[[nodiscard]] std::span<const std::string_view> eggs18();
[[nodiscard]] std::span<const std::string_view> potions18();
/// The tags a 1.13 server sends, which 1.12.2 had none of.
[[nodiscard]] std::span<const std::uint8_t> tags();

} // namespace basalt::wire::multiversion::java::flattening340
