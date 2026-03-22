#include <basalt/wire/bedrock/BedrockItems.hpp>

#include <algorithm>
#include <ranges>

namespace basalt::wire::bedrock {

const ItemName* bedrockItem(const std::string_view javaItem) {
    const std::span<const ItemName> items = bedrockItems();
    const auto found = std::ranges::lower_bound(items, javaItem, {}, &ItemName::java);
    if (found == items.end() || found->java != javaItem) {
        return nullptr;
    }
    return &*found;
}

const ItemStandIn* bedrockItemStandIn(const std::int32_t palettePlace, const int protocol) {
    const std::span<const ItemStandIn> standIns = bedrockItemStandIns();
    const auto found = std::ranges::lower_bound(standIns, palettePlace, {}, &ItemStandIn::palettePlace);
    if (found == standIns.end() || found->palettePlace != palettePlace || protocol >= found->since) {
        return nullptr;
    }
    return &*found;
}

bool bedrockItemNameKnown(const std::string_view name, const int protocol) {
    const std::span<const std::string_view> palette = bedrockItemPalette();
    const auto found = std::ranges::lower_bound(palette, name);
    if (found == palette.end() || *found != name) {
        return true;
    }
    return bedrockItemStandIn(static_cast<std::int32_t>(found - palette.begin()), protocol) == nullptr;
}

} // namespace basalt::wire::bedrock
