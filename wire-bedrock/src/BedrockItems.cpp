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

} // namespace basalt::wire::bedrock
