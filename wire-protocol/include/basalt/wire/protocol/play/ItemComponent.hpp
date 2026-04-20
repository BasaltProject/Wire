#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace basalt::wire::protocol::play {

namespace dataComponent {
inline constexpr std::int32_t kCustomData = 0;
inline constexpr std::int32_t kMaxStackSize = 1;
inline constexpr std::int32_t kMaxDamage = 2;
inline constexpr std::int32_t kDamage = 3;
inline constexpr std::int32_t kUnbreakable = 4;
inline constexpr std::int32_t kCustomName = 6;
inline constexpr std::int32_t kItemName = 9;
inline constexpr std::int32_t kLore = 11;
inline constexpr std::int32_t kRarity = 12;
inline constexpr std::int32_t kEnchantments = 13;
inline constexpr std::int32_t kRepairCost = 19;
inline constexpr std::int32_t kCreativeSlotLock = 20;
inline constexpr std::int32_t kEnchantmentGlintOverride = 21;
inline constexpr std::int32_t kStoredEnchantments = 45;
inline constexpr std::int32_t kDyedColor = 47;
} // namespace dataComponent

/// parity: `DataComponentPatch`.
class ItemComponent {
public:
    struct Enchantments {
        std::vector<std::pair<std::int32_t, std::int32_t>> levels;
        bool stored = false;

        friend bool operator==(const Enchantments&, const Enchantments&) = default;
    };

    struct Number {
        std::int32_t value = 0;

        friend bool operator==(const Number&, const Number&) = default;
    };

    struct Color {
        std::int32_t rgb = 0;

        friend bool operator==(const Color&, const Color&) = default;
    };

    struct Text {
        text::Component text;

        friend bool operator==(const Text&, const Text&) = default;
    };

    /// `lore`: a list of components, at most 256 as vanilla's own codec allows.
    struct Lore {
        std::vector<text::Component> lines;

        friend bool operator==(const Lore&, const Lore&) = default;
    };

    struct CustomData {
        nbt::NbtCompound tag;

        friend bool operator==(const CustomData&, const CustomData&) = default;
    };

    /// `enchantment_glint_override`, the only boolean component.
    struct Flag {
        bool value = false;

        friend bool operator==(const Flag&, const Flag&) = default;
    };

    struct Unit {
        friend bool operator==(const Unit&, const Unit&) = default;
    };

    struct Raw {
        std::vector<std::uint8_t> payload;

        friend bool operator==(const Raw&, const Raw&) = default;
    };

    using Payload = std::variant<Enchantments, Number, Color, Text, Lore, CustomData, Flag, Unit, Raw>;

    ItemComponent(std::int32_t id, Payload payload) : id_(id), payload_(std::move(payload)) {}

    [[nodiscard]] std::int32_t id() const noexcept { return id_; }

    [[nodiscard]] const Payload& payload() const noexcept { return payload_; }

    template <typename T>
    [[nodiscard]] const T* as() const noexcept {
        return std::get_if<T>(&payload_);
    }

    void writePayload(io::ByteBuffer& buffer) const;

    [[nodiscard]] static std::optional<ItemComponent> readPayload(io::ByteBuffer& buffer, std::int32_t id,
                                                                  std::optional<std::size_t> length);

    friend bool operator==(const ItemComponent&, const ItemComponent&) = default;

private:
    std::int32_t id_;
    Payload payload_;
};

} // namespace basalt::wire::protocol::play
