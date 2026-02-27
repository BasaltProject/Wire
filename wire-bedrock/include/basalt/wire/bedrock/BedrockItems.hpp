#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::wire::bedrock {

/// One Java item and what it is on the other edition: which name in the palette, and which aux
/// value, for the handful of names that cover more than one Java item.
struct ItemName {
    std::string_view java;
    std::int32_t palettePlace = 0;
    std::int32_t data = 0;
};

/// The names this server declares to a client, in the order it declares them. An item's id on the
/// wire is its place here plus one: Bedrock has let the server name its own item ids since 776, so
/// there is no per-version table of them to keep.
[[nodiscard]] std::span<const std::string_view> bedrockItemPalette();

/// Java's items, sorted by name.
[[nodiscard]] std::span<const ItemName> bedrockItems();

/// What a Java item is on the wire: its id and its aux value, or nothing when that edition has no
/// such item. A count of zero is air whatever the id says.
[[nodiscard]] const ItemName* bedrockItem(std::string_view javaItem);

} // namespace basalt::wire::bedrock
