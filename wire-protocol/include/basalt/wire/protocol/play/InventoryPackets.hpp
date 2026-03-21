#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::protocol::play {

/// One stack on the wire: item, count and its components as a `DataComponentPatch`, so a stack a
/// client echoes back comes out as the one that went in. An unknown component is kept as bytes only
/// in the delimited form; elsewhere it is refused, since skipping it blindly would desynchronise.
struct WireItemStack {
    std::int32_t itemId = 0;
    std::int32_t count = 0;
    /// What this stack adds to its item's defaults, in the order it was written.
    std::vector<ItemComponent> components;
    /// Component ids this stack removes from its item's defaults.
    std::vector<std::int32_t> removedComponents;

    /// The `minecraft:enchantments` levels, or empty: by far the most asked-for component.
    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> enchantments() const;

    /// The `minecraft:damage` this stack has taken, or zero when it carries none.
    [[nodiscard]] std::int32_t damage() const;

    void write(io::ByteBuffer& buffer) const;

    /// A count of zero is how vanilla writes "this slot is empty", and the rest of the stack follows
    /// only when it is not.
    static void writeOptional(io::ByteBuffer& buffer, const std::optional<WireItemStack>& stack);

    [[nodiscard]] static std::optional<WireItemStack> read(io::ByteBuffer& buffer);

    /// The same stack in the length-prefixed form `set_creative_slot` uses, so unknown components are
    /// kept: a creative client echoes its slots whenever the inventory opens.
    [[nodiscard]] static std::optional<WireItemStack> readDelimited(io::ByteBuffer& buffer);

    friend bool operator==(const WireItemStack&, const WireItemStack&) = default;
};

/// Every slot of an open container at once.
///
/// Container 0 is the player's own inventory. The state id is vanilla's optimistic-locking counter:
/// the client echoes it back with every click, so the server can tell a click made against the
/// current contents from one made against contents it has since replaced.
struct ContainerSetContentPacket {
    static constexpr std::int32_t kId = 0x12;
    static constexpr std::int32_t kPlayerInventory = 0;

    std::int32_t containerId = kPlayerInventory;
    std::int32_t stateId = 0;
    std::vector<std::optional<WireItemStack>> items;
    std::optional<WireItemStack> carriedItem;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ContainerSetContentPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ContainerSetContentPacket&, const ContainerSetContentPacket&) = default;
};

/// One slot of one container, for when a single thing changed.
struct ContainerSetSlotPacket {
    static constexpr std::int32_t kId = 0x14;

    std::int32_t containerId = 0;
    std::int32_t stateId = 0;
    std::int16_t slot = 0;
    std::optional<WireItemStack> item;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ContainerSetSlotPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ContainerSetSlotPacket&, const ContainerSetSlotPacket&) = default;
};

/// Which hotbar slot the player is holding, server to client.
struct SetHeldSlotPacket {
    static constexpr std::int32_t kId = 0x6b;

    std::int32_t slot = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetHeldSlotPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetHeldSlotPacket&, const SetHeldSlotPacket&) = default;
};

/// The same, client to server: the player turned the mouse wheel or pressed a number key.
struct ServerboundSetCarriedItemPacket {
    static constexpr std::int32_t kId = 0x36;
    /// Hotbar slots only; anything else is a client sending nonsense.
    static constexpr std::int16_t kHotbarSize = 9;

    std::int16_t slot = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundSetCarriedItemPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundSetCarriedItemPacket&, const ServerboundSetCarriedItemPacket&) = default;
};

/// Serverbound, id 0x12: a click inside an open container. The changed slots and carried item are the
/// client's own prediction, sent as component hashes; this build does not verify them, since the
/// click is re-derived against the server's own inventory, which is authoritative anyway.
///
/// They are still parsed exactly, because a stack skipped wrong desynchronises every packet after it.
struct ServerboundContainerClickPacket {
    static constexpr std::int32_t kId = 0x12;

    /// parity: ContainerInput, by its own numeric ids.
    enum class Mode : std::uint8_t {
        Pickup,
        QuickMove,
        Swap,
        Clone,
        Throw,
        QuickCraft,
        PickupAll,
    };

    std::int32_t containerId = 0;
    std::int32_t stateId = 0;
    std::int16_t slot = 0;
    std::int8_t button = 0;
    Mode mode = Mode::Pickup;
    /// Whether the client believes it is holding something after this click. Its full predicted
    /// stack is hashed and not reconstructible here, but the flag alone is enough to notice that the
    /// two sides disagree about whether a stack is in hand at all, which is the shape a click
    /// desynchronisation takes.
    bool clientHasCarriedItem = false;

    [[nodiscard]] static std::optional<ServerboundContainerClickPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundContainerClickPacket&, const ServerboundContainerClickPacket&) = default;
};

/// Serverbound, id 0x39: creative-mode inventory editing.
///
/// This is the only way a creative client's dragged item reaches the server, whether it came from
/// the creative menu into a slot or from a slot into the void, which arrives as slot -1. Creative
/// item movement never goes through a window-click packet.
struct ServerboundSetCreativeModeSlotPacket {
    static constexpr std::int32_t kId = 0x39;

    std::int16_t slot = 0;
    std::optional<WireItemStack> item;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundSetCreativeModeSlotPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundSetCreativeModeSlotPacket&,
                           const ServerboundSetCreativeModeSlotPacket&) = default;
};

/// The numeric ids of the `minecraft:menu` registry. The client picks the screen *and the slot
/// layout* from this, so the server's own idea of the layout has to agree with what is sent.
namespace menuType {
inline constexpr std::int32_t kGeneric9x3 = 2;
inline constexpr std::int32_t kGeneric9x6 = 5;
inline constexpr std::int32_t kCrafter3x3 = 7;
inline constexpr std::int32_t kAnvil = 8;
inline constexpr std::int32_t kBeacon = 9;
inline constexpr std::int32_t kBlastFurnace = 10;
inline constexpr std::int32_t kBrewingStand = 11;
inline constexpr std::int32_t kCrafting = 12;
inline constexpr std::int32_t kEnchantment = 13;
inline constexpr std::int32_t kFurnace = 14;
inline constexpr std::int32_t kGrindstone = 15;
inline constexpr std::int32_t kLectern = 17;
inline constexpr std::int32_t kLoom = 18;
inline constexpr std::int32_t kMerchant = 19;
inline constexpr std::int32_t kSmithing = 21;
inline constexpr std::int32_t kSmoker = 22;
inline constexpr std::int32_t kStonecutter = 24;
} // namespace menuType

/// Clientbound, id 0x3c: opens a container's screen on the client.
struct OpenScreenPacket {
    static constexpr std::int32_t kId = 0x3c;

    std::int32_t containerId = 0;
    std::int32_t menuType = menuType::kGeneric9x3;
    text::Component title;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<OpenScreenPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const OpenScreenPacket&, const OpenScreenPacket&) = default;
};

/// The container was closed. Both directions carry the same body and both exist: the client says so
/// when the player presses escape, and the server says so when it closes one on their behalf.
struct ContainerClosePacket {
    static constexpr std::int32_t kClientboundId = 0x11;
    static constexpr std::int32_t kServerboundId = 0x13;

    std::int32_t containerId = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ContainerClosePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ContainerClosePacket&, const ContainerClosePacket&) = default;
};

/// Clientbound, id 0x13: one number of an open window, which is how a furnace draws its two bars.
///
/// What the id means is the window's own business: for a furnace, 0 is the fire left, 1 is how long
/// that fuel lasts, 2 is the progress and 3 the total.
struct ContainerSetDataPacket {
    static constexpr std::int32_t kId = 0x13;

    /// parity: `BeaconMenu`'s three: the pyramid, then the two effects as registry ids shifted up
    /// one so that zero can mean "none".
    static constexpr std::int16_t kBeaconLevels = 0;
    static constexpr std::int16_t kBeaconPrimary = 1;
    static constexpr std::int16_t kBeaconSecondary = 2;

    /// parity: `LoomMenu`'s one data slot, which row of patterns is picked. -1 is none.
    static constexpr std::int16_t kLoomPattern = 0;

    /// parity: `AnvilMenu`'s one data slot, the levels it is asking for.
    static constexpr std::int16_t kAnvilCost = 0;

    /// parity: `EnchantmentMenu`'s ten: three costs, the seed, then the three enchantments it is
    /// hinting at and the three levels they would come at.
    static constexpr std::int16_t kEnchantmentCost = 0;
    static constexpr std::int16_t kEnchantmentSeed = 3;
    static constexpr std::int16_t kEnchantmentClue = 4;
    static constexpr std::int16_t kEnchantmentLevelClue = 7;

    /// parity: AbstractFurnaceMenu's own four data slots.
    static constexpr std::int16_t kFurnaceLitTime = 0;
    static constexpr std::int16_t kFurnaceLitDuration = 1;
    static constexpr std::int16_t kFurnaceCookingProgress = 2;
    static constexpr std::int16_t kFurnaceCookingTotal = 3;

    std::int32_t containerId = 0;
    std::int16_t id = 0;
    std::int16_t value = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ContainerSetDataPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ContainerSetDataPacket&, const ContainerSetDataPacket&) = default;
};

/// One thing a merchant asks for: an item and how many.
struct MerchantCost {
    std::int32_t itemId = 0;
    std::int32_t count = 1;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static MerchantCost read(io::ByteBuffer& buffer);

    friend bool operator==(const MerchantCost&, const MerchantCost&) = default;
};

/// One offer as the client draws it.
struct MerchantOffer {
    MerchantCost costA;
    WireItemStack result;
    std::optional<MerchantCost> costB;
    bool outOfStock = false;
    std::int32_t uses = 0;
    std::int32_t maxUses = 0;
    std::int32_t experience = 0;
    std::int32_t specialPrice = 0;
    float priceMultiplier = 0.0F;
    std::int32_t demand = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<MerchantOffer> read(io::ByteBuffer& buffer);

    friend bool operator==(const MerchantOffer&, const MerchantOffer&) = default;
};

/// Clientbound, id 0x35: the trades a merchant is offering.
///
/// Sent right after the merchant window opens, and again whenever a trade is taken: the client draws
/// the list it was last sent and nothing else.
struct MerchantOffersPacket {
    static constexpr std::int32_t kId = 0x35;

    std::int32_t containerId = 0;
    std::vector<MerchantOffer> offers;
    std::int32_t level = 1;
    std::int32_t experience = 0;
    bool showProgress = false;
    bool canRestock = true;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<MerchantOffersPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const MerchantOffersPacket&, const MerchantOffersPacket&) = default;
};

/// Serverbound, id 0x34: which of the offers the player picked.
struct SelectTradePacket {
    static constexpr std::int32_t kId = 0x34;

    std::int32_t index = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SelectTradePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SelectTradePacket&, const SelectTradePacket&) = default;
};

/// Serverbound, id 0x31: what the player typed into an anvil's name box. It arrives on every
/// keystroke, and the result is worked out again each time.
struct ServerboundRenameItemPacket {
    static constexpr std::int32_t kId = 0x31;

    /// parity: `AnvilMenu.MAX_NAME_LENGTH`, past which the jar ignores the name outright.
    static constexpr std::size_t kMaxLength = 50;

    std::string name;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundRenameItemPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundRenameItemPacket&, const ServerboundRenameItemPacket&) = default;
};

/// Serverbound, id 0x18: the pages a player typed into a book, and the title when they signed it.
///
/// parity: `ServerboundEditBookPacket`, whose slot is the player's own inventory numbering: the
/// hotbar is nought to eight and forty is the off hand.
struct ServerboundEditBookPacket {
    static constexpr std::int32_t kId = 0x18;

    /// parity: the codec's own limits, which a packet past them is refused for.
    static constexpr std::size_t kMaxPages = 100;
    static constexpr std::int32_t kMaxPageLength = 1024;
    static constexpr std::int32_t kMaxTitleLength = 32;

    std::int32_t slot = 0;
    std::vector<std::string> pages;
    /// Set only when they signed it, which is what turns the book into a written one.
    std::optional<std::string> title;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundEditBookPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundEditBookPacket&, const ServerboundEditBookPacket&) = default;
};

/// Serverbound, id 0x35: the two effects picked in a beacon's window, each of them a mob effect's
/// registry id. parity: `ServerboundSetBeaconPacket`, whose two fields are both optional because
/// "none" is a choice the window can make.
struct ServerboundSetBeaconPacket {
    static constexpr std::int32_t kId = 0x35;

    std::optional<std::int32_t> primary;
    std::optional<std::int32_t> secondary;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundSetBeaconPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundSetBeaconPacket&, const ServerboundSetBeaconPacket&) = default;
};

/// Serverbound, id 0x11: a button inside an open window.
///
/// The lectern is what makes this worth having: its pages are buttons, one back, two forward, three
/// to take the book, and a hundred plus the page to jump straight to one.
struct ContainerButtonClickPacket {
    static constexpr std::int32_t kId = 0x11;

    /// parity: LecternMenu.clickMenuButton's own numbering.
    static constexpr std::int32_t kLecternPreviousPage = 1;
    static constexpr std::int32_t kLecternNextPage = 2;
    static constexpr std::int32_t kLecternTakeBook = 3;
    static constexpr std::int32_t kLecternJumpToPage = 100;

    std::int32_t containerId = 0;
    std::int32_t buttonId = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ContainerButtonClickPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ContainerButtonClickPacket&, const ContainerButtonClickPacket&) = default;
};

/// Serverbound, id 0x14: a slot of the open window switched on or off.
///
/// Only the crafter has such slots: a switched-off slot takes nothing and a comparator counts it as
/// full, so this is how a player picks which shape the crafter builds.
struct ContainerSlotStateChangedPacket {
    static constexpr std::int32_t kId = 0x14;

    std::int32_t slotId = 0;
    std::int32_t containerId = 0;
    bool newState = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ContainerSlotStateChangedPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ContainerSlotStateChangedPacket&, const ContainerSlotStateChangedPacket&) = default;
};

} // namespace basalt::wire::protocol::play
