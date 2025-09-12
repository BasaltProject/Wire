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

/// The data component ids this build knows how to write. parity: the registration order of
/// DataComponents.
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
inline constexpr std::int32_t kStoredEnchantments = 42;
inline constexpr std::int32_t kDyedColor = 44;
} // namespace dataComponent

/// One entry of an item stack's component patch. parity: DataComponentPatch, whose wire form is the
/// added components and then the removed ones, each by its registry id.
///
/// The raw case is what keeps this honest. A component with no case here is carried as the bytes it
/// arrived as instead of being dropped, so a stack that goes out to a client and comes back is the
/// same stack; only writing a *new* value of one needs a case, and those are the ones a command can
/// set.
class ItemComponent {
public:
    /// Enchantment registry id to level, for `enchantments` and `stored_enchantments`.
    struct Enchantments {
        std::vector<std::pair<std::int32_t, std::int32_t>> levels;
        bool stored = false;

        friend bool operator==(const Enchantments&, const Enchantments&) = default;
    };

    /// A component whose whole payload is one varint: damage, max_damage, max_stack_size,
    /// repair_cost, rarity.
    struct Number {
        std::int32_t value = 0;

        friend bool operator==(const Number&, const Number&) = default;
    };

    /// `dyed_color`, a plain big-endian int rather than a varint.
    struct Color {
        std::int32_t rgb = 0;

        friend bool operator==(const Color&, const Color&) = default;
    };

    /// `custom_name` and `item_name`.
    struct Text {
        text::Component text;

        friend bool operator==(const Text&, const Text&) = default;
    };

    /// `lore`: a list of components, at most 256 as vanilla's own codec allows.
    struct Lore {
        std::vector<text::Component> lines;

        friend bool operator==(const Lore&, const Lore&) = default;
    };

    /// `custom_data`: a compound nothing but a datapack reads.
    struct CustomData {
        nbt::NbtCompound tag;

        friend bool operator==(const CustomData&, const CustomData&) = default;
    };

    /// `enchantment_glint_override`, the only boolean component.
    struct Flag {
        bool value = false;

        friend bool operator==(const Flag&, const Flag&) = default;
    };

    /// Present or absent and carrying nothing: `unbreakable`, `creative_slot_lock`.
    struct Unit {
        friend bool operator==(const Unit&, const Unit&) = default;
    };

    /// A component this build has no codec for, kept as the bytes it arrived as.
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

    /// Reads one component's payload, given how many bytes it occupies, or nothing when the length
    /// is unknown and this build has no codec for that id.
    ///
    /// The typed cases are the ones a command can set, so they are also the ones that have to
    /// survive a round trip through a client rather than coming back as opaque bytes.
    [[nodiscard]] static std::optional<ItemComponent> readPayload(io::ByteBuffer& buffer, std::int32_t id,
                                                                  std::optional<std::size_t> length);

    friend bool operator==(const ItemComponent&, const ItemComponent&) = default;

private:
    std::int32_t id_;
    Payload payload_;
};

} // namespace basalt::wire::protocol::play
