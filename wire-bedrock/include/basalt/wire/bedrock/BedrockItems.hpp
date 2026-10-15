#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::wire::bedrock {

struct ItemName {
    std::string_view java;
    std::int32_t palettePlace = 0;
    std::int32_t data = 0;
};

struct ItemStandIn {
    std::int32_t palettePlace = 0;
    int since = 0;
    std::int32_t standInPlace = 0;
    std::int32_t standInData = 0;
};

/// The names this server declares to a client, in the order it declares them.
[[nodiscard]] std::span<const std::string_view> bedrockItemPalette();

/// Every name some older client lacks, sorted by palette place.
[[nodiscard]] std::span<const ItemStandIn> bedrockItemStandIns();

[[nodiscard]] const ItemStandIn* bedrockItemStandIn(std::int32_t palettePlace, int protocol);

/// Whether a client of this protocol knows a palette name.
[[nodiscard]] bool bedrockItemNameKnown(std::string_view name, int protocol);

[[nodiscard]] std::span<const ItemName> bedrockItems();

[[nodiscard]] const ItemName* bedrockItem(std::string_view javaItem);

} // namespace basalt::wire::bedrock
