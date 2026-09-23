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

/// A palette name older clients lack, from which protocol on they have it, and what they are shown
/// until then.
struct ItemStandIn {
    std::int32_t palettePlace = 0;
    int since = 0;
    std::int32_t standInPlace = 0;
    std::int32_t standInData = 0;
};

/// The names this server declares to a client, in the order it declares them. An item's id on the
/// wire is its place here plus one: Bedrock has let the server name its own item ids since 776, so
/// there is no per-version table of them to keep.
[[nodiscard]] std::span<const std::string_view> bedrockItemPalette();

/// Every name some older client lacks, sorted by palette place.
[[nodiscard]] std::span<const ItemStandIn> bedrockItemStandIns();

/// What a client of this protocol is shown in place of the item at that palette place, or nothing
/// when it knows that item itself.
[[nodiscard]] const ItemStandIn* bedrockItemStandIn(std::int32_t palettePlace, int protocol);

/// Whether a client of this protocol knows a palette name. A name not in the palette at all is not
/// this server's to judge, and counts as known.
[[nodiscard]] bool bedrockItemNameKnown(std::string_view name, int protocol);

/// Java's items, sorted by name.
[[nodiscard]] std::span<const ItemName> bedrockItems();

/// What a Java item is on the wire: its id and its aux value, or nothing when that edition has no
/// such item. A count of zero is air whatever the id says.
[[nodiscard]] const ItemName* bedrockItem(std::string_view javaItem);

} // namespace basalt::wire::bedrock
