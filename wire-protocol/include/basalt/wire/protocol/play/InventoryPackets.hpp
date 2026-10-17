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

struct WireItemStack {
    std::int32_t itemId = 0;
    std::int32_t count = 0;
    /// What this stack adds to its item's defaults, in the order it was written.
    std::vector<ItemComponent> components;
    std::vector<std::int32_t> removedComponents;

    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> enchantments() const;

    [[nodiscard]] std::int32_t damage() const;

    void write(io::ByteBuffer& buffer) const;

    static void writeOptional(io::ByteBuffer& buffer, const std::optional<WireItemStack>& stack);

    [[nodiscard]] static std::optional<WireItemStack> read(io::ByteBuffer& buffer);

    [[nodiscard]] static std::optional<WireItemStack> readDelimited(io::ByteBuffer& buffer);

    friend bool operator==(const WireItemStack&, const WireItemStack&) = default;
};

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

struct ServerboundContainerClickPacket {
    static constexpr std::int32_t kId = 0x12;

    /// parity: ContainerInput.
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
    /// Whether the client believes it is holding something after this click.
    bool clientHasCarriedItem = false;

    /// Written without the hashes of what the client expects, so the backend answers with what it really holds.
    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundContainerClickPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundContainerClickPacket&, const ServerboundContainerClickPacket&) = default;
};

/// Creative item movement never goes through a window-click packet.
struct ServerboundSetCreativeModeSlotPacket {
    static constexpr std::int32_t kId = 0x39;

    std::int16_t slot = 0;
    std::optional<WireItemStack> item;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundSetCreativeModeSlotPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundSetCreativeModeSlotPacket&,
                           const ServerboundSetCreativeModeSlotPacket&) = default;
};

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

struct ContainerClosePacket {
    static constexpr std::int32_t kClientboundId = 0x11;
    static constexpr std::int32_t kServerboundId = 0x13;

    std::int32_t containerId = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ContainerClosePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ContainerClosePacket&, const ContainerClosePacket&) = default;
};

struct ContainerSetDataPacket {
    static constexpr std::int32_t kId = 0x13;

    /// parity: `BeaconMenu`'s three: the pyramid, then the two effects as registry ids shifted up.
    static constexpr std::int16_t kBeaconLevels = 0;
    static constexpr std::int16_t kBeaconPrimary = 1;
    static constexpr std::int16_t kBeaconSecondary = 2;

    /// parity: `LoomMenu`'s one data slot, which row of patterns is picked. -1 is none.
    static constexpr std::int16_t kLoomPattern = 0;

    static constexpr std::int16_t kAnvilCost = 0;

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

struct MerchantCost {
    std::int32_t itemId = 0;
    std::int32_t count = 1;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static MerchantCost read(io::ByteBuffer& buffer);

    friend bool operator==(const MerchantCost&, const MerchantCost&) = default;
};

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

struct SelectTradePacket {
    static constexpr std::int32_t kId = 0x34;

    std::int32_t index = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SelectTradePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SelectTradePacket&, const SelectTradePacket&) = default;
};

struct ServerboundRenameItemPacket {
    static constexpr std::int32_t kId = 0x31;

    /// parity: `AnvilMenu.MAX_NAME_LENGTH`.
    static constexpr std::size_t kMaxLength = 50;

    std::string name;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundRenameItemPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundRenameItemPacket&, const ServerboundRenameItemPacket&) = default;
};

/// parity: `ServerboundEditBookPacket`.
struct ServerboundEditBookPacket {
    static constexpr std::int32_t kId = 0x18;

    static constexpr std::size_t kMaxPages = 100;
    static constexpr std::int32_t kMaxPageLength = 1024;
    static constexpr std::int32_t kMaxTitleLength = 32;

    std::int32_t slot = 0;
    std::vector<std::string> pages;
    std::optional<std::string> title;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundEditBookPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundEditBookPacket&, const ServerboundEditBookPacket&) = default;
};

struct ServerboundSetBeaconPacket {
    static constexpr std::int32_t kId = 0x35;

    std::optional<std::int32_t> primary;
    std::optional<std::int32_t> secondary;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundSetBeaconPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundSetBeaconPacket&, const ServerboundSetBeaconPacket&) = default;
};

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
