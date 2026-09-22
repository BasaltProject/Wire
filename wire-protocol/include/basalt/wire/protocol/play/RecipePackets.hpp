#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace basalt::wire::protocol::play {

/// One slot of a recipe as the book draws it. parity: SlotDisplay, of which the shapes a crafting
/// recipe needs are here: nothing, one item, or a choice between several.
///
/// The type ids are SlotDisplays' own registration order, which is what the wire carries.
class SlotDisplay {
public:
    static constexpr std::int32_t kEmptyType = 0;
    static constexpr std::int32_t kItemType = 4;
    static constexpr std::int32_t kCompositeType = 10;
    /// parity: SlotDisplays.bootstrap, where `item_stack` is the sixth registered.
    static constexpr std::int32_t kItemStackType = 5;

    /// An empty cell of a shaped recipe.
    struct Empty {
        friend bool operator==(const Empty&, const Empty&) = default;
    };

    /// One item, by its protocol id.
    struct Item {
        std::int32_t itemId = 0;

        friend bool operator==(const Item&, const Item&) = default;
    };

    /// Any of several items, which is what an ingredient built from a tag looks like.
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

    /// Writes the type and then the body, which is how every one of these travels.
    void write(io::ByteBuffer& buffer) const;

    [[nodiscard]] static std::optional<SlotDisplay> read(io::ByteBuffer& buffer);

    friend bool operator==(const SlotDisplay&, const SlotDisplay&);

private:
    Payload payload_;
};

/// A recipe as the book draws it. parity: RecipeDisplay, limited to the two crafting shapes this
/// server understands; the type ids are RecipeDisplays' registration order.
struct RecipeDisplay {
    static constexpr std::int32_t kShapelessType = 0;
    static constexpr std::int32_t kShapedType = 1;

    struct Grid {
        std::int32_t width = 0;
        std::int32_t height = 0;

        friend bool operator==(const Grid&, const Grid&) = default;
    };

    /// Present exactly when the recipe is shaped. A shapeless recipe has no grid at all, and giving
    /// it two fields that the wire then drops is a state that only ever produces bugs.
    std::optional<Grid> grid;
    std::vector<SlotDisplay> ingredients;
    SlotDisplay result;
    SlotDisplay craftingStation;

    [[nodiscard]] std::int32_t type() const noexcept { return grid ? kShapedType : kShapelessType; }

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RecipeDisplay> read(io::ByteBuffer& buffer);

    friend bool operator==(const RecipeDisplay&, const RecipeDisplay&) = default;
};

/// One entry of the recipe book. The crafting requirements are what the book highlights with: each
/// ingredient is a holder set of item ids, and a recipe with none can be shown but never suggested.
struct RecipeBookEntry {
    /// parity: RecipeBookCategories' registration order; `crafting_misc` is what a command adds to.
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

/// Clientbound, id 0x4b: adds recipes to the book.
struct RecipeBookAddPacket {
    static constexpr std::int32_t kId = 0x4b;

    std::vector<RecipeBookEntry> entries;
    bool replace = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RecipeBookAddPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const RecipeBookAddPacket&, const RecipeBookAddPacket&) = default;
};

/// Clientbound, id 0x4c: takes recipes back out of the book.
struct RecipeBookRemovePacket {
    static constexpr std::int32_t kId = 0x4c;

    std::vector<std::int32_t> recipeIds;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RecipeBookRemovePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const RecipeBookRemovePacket&, const RecipeBookRemovePacket&) = default;
};

/// One row of a stonecutter's list: what goes in, and the stack the button shows.
struct StonecutterOption {
    std::vector<std::int32_t> inputItemIds;
    std::int32_t resultItemId = 0;
    std::int32_t resultCount = 1;

    friend bool operator==(const StonecutterOption&, const StonecutterOption&) = default;
};

/// Clientbound, id 0x88: the recipes a client needs to have itself.
///
/// The stonecutter is why it exists: the client draws its buttons from this list and sends back an
/// index into it. The item property sets are the recipe book's highlighting and go out empty.
struct UpdateRecipesPacket {
    static constexpr std::int32_t kId = 0x88;

    std::vector<StonecutterOption> stonecutter;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<UpdateRecipesPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const UpdateRecipesPacket&, const UpdateRecipesPacket&) = default;
};

} // namespace basalt::wire::protocol::play
