#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {

/// The tag a stack carried before 1.20.5 turned it into components, and back.
namespace legacyItemTags {

inline constexpr std::int32_t kPotion = 1271;

/// Whether this server's item is a potion, the one item 1.8 told apart by its data value alone.
[[nodiscard]] bool isPotion(std::int32_t nativeId) noexcept;

/// Turns a stack a client sent, back in this server's numbering, from its tag and data value into components.
void fromStack(protocol::play::WireItemStack& item, std::int32_t data, std::int32_t farEnd,
               std::span<const std::string> enchantments);

/// `enchantments` names this server's enchantment ids, in the order its registry data listed them.
[[nodiscard]] nbt::NbtCompound toTag(const protocol::play::WireItemStack& item, std::int32_t farEnd,
                                     std::span<const std::string> enchantments);

[[nodiscard]] std::vector<protocol::play::ItemComponent> toComponents(const nbt::NbtCompound& tag, std::int32_t farEnd,
                                                                      std::span<const std::string> enchantments);

/// A stack already numbered for `farEnd`, left with its tag alone, carried down the links as its custom data.
[[nodiscard]] protocol::play::WireItemStack toStack(protocol::play::WireItemStack item, std::int32_t nativeId,
                                                    std::int32_t farEnd, std::span<const std::string> enchantments);

/// The enchantment keys of a registry data packet, if it is the one for enchantments.
[[nodiscard]] std::optional<std::vector<std::string>> enchantmentNames(const std::vector<std::uint8_t>& body);

} // namespace legacyItemTags

} // namespace basalt::wire::multiversion::java
