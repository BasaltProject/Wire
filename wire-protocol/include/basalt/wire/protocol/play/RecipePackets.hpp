#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace basalt::wire::protocol::play {

/// parity: SlotDisplay.
class SlotDisplay {
public:
    static constexpr std::int32_t kEmptyType = 0;
    static constexpr std::int32_t kItemType = 4;
    static constexpr std::int32_t kCompositeType = 10;
    /// parity: SlotDisplays.bootstrap.
    static constexpr std::int32_t kItemStackType = 5;

    struct Empty {
        friend bool operator==(const Empty&, const Empty&) = default;
    };

    struct Item {
        std::int32_t itemId = 0;

        friend bool operator==(const Item&, const Item&) = default;
    };

    struct Composite {
        std::vector<SlotDisplay> options;

        friend bool operator==(const Composite&, const Composite&);
    };

    using Payload = std::variant<Empty, Item, Composite>;

    SlotDisplay();
    explicit SlotDisplay(Payload payload);
    SlotDisplay(const SlotDisplay&);
    SlotDisplay(SlotDisplay&&) noexcept;
    SlotDisplay& operator=(const SlotDisplay&);
    SlotDisplay& operator=(SlotDisplay&&) noexcept;
    ~SlotDisplay();

    [[nodiscard]] std::int32_t type() const noexcept;

    [[nodiscard]] const Payload& payload() const noexcept { return payload_; }

    template <typename T>
    [[nodiscard]] const T* as() const noexcept {
        return std::get_if<T>(&payload_);
    }

    void write(io::ByteBuffer& buffer) const;

    [[nodiscard]] static std::optional<SlotDisplay> read(io::ByteBuffer& buffer);

    friend bool operator==(const SlotDisplay&, const SlotDisplay&);

private:
    Payload payload_;
};

/// parity: RecipeDisplay.
struct RecipeDisplay {
    static constexpr std::int32_t kShapelessType = 0;
    static constexpr std::int32_t kShapedType = 1;

    struct Grid {
        std::int32_t width = 0;
        std::int32_t height = 0;

        friend bool operator==(const Grid&, const Grid&) = default;
    };

    /// Present exactly when the recipe is shaped.
    std::optional<Grid> grid;
    std::vector<SlotDisplay> ingredients;
    SlotDisplay result;
    SlotDisplay craftingStation;

    [[nodiscard]] std::int32_t type() const noexcept { return grid ? kShapedType : kShapelessType; }

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RecipeDisplay> read(io::ByteBuffer& buffer);

    friend bool operator==(const RecipeDisplay&, const RecipeDisplay&) = default;
};

struct RecipeBookEntry {
    static constexpr std::int32_t kCraftingMiscCategory = 3;

    std::int32_t recipeId = 0;
    RecipeDisplay display;
    std::int32_t category = kCraftingMiscCategory;
    std::vector<std::vector<std::int32_t>> craftingRequirements;
    bool notification = false;
    bool highlight = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RecipeBookEntry> read(io::ByteBuffer& buffer);

    friend bool operator==(const RecipeBookEntry&, const RecipeBookEntry&) = default;
};

struct RecipeBookAddPacket {
    static constexpr std::int32_t kId = 0x4b;

    std::vector<RecipeBookEntry> entries;
    bool replace = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RecipeBookAddPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const RecipeBookAddPacket&, const RecipeBookAddPacket&) = default;
};

/// Serverbound, id 0x27: the recipe book asking to fill the open crafting grid.
struct PlaceRecipePacket {
    static constexpr std::int32_t kId = 0x27;

    std::int32_t containerId = 0;
    std::int32_t recipeId = 0;
    bool useMaxItems = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PlaceRecipePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlaceRecipePacket&, const PlaceRecipePacket&) = default;
};

/// Clientbound, id 0x40: shows the recipe as ghost items when the ingredients are missing.
struct PlaceGhostRecipePacket {
    static constexpr std::int32_t kId = 0x40;

    std::int32_t containerId = 0;
    RecipeDisplay display;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PlaceGhostRecipePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlaceGhostRecipePacket&, const PlaceGhostRecipePacket&) = default;
};

struct RecipeBookRemovePacket {
    static constexpr std::int32_t kId = 0x4c;

    std::vector<std::int32_t> recipeIds;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RecipeBookRemovePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const RecipeBookRemovePacket&, const RecipeBookRemovePacket&) = default;
};

struct StonecutterOption {
    std::vector<std::int32_t> inputItemIds;
    std::int32_t resultItemId = 0;
    std::int32_t resultCount = 1;

    friend bool operator==(const StonecutterOption&, const StonecutterOption&) = default;
};

struct ItemPropertySet {
    std::string key;
    std::vector<std::int32_t> itemIds;

    friend bool operator==(const ItemPropertySet&, const ItemPropertySet&) = default;
};

/// Clientbound, id 0x88: the recipes a client needs to have itself.
struct UpdateRecipesPacket {
    static constexpr std::int32_t kId = 0x88;

    std::vector<ItemPropertySet> propertySets;
    std::vector<StonecutterOption> stonecutter;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<UpdateRecipesPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const UpdateRecipesPacket&, const UpdateRecipesPacket&) = default;
};

} // namespace basalt::wire::protocol::play
