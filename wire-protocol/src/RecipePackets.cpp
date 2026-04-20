#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>

#include <cstdint>

namespace basalt::wire::protocol::play {

bool operator==(const SlotDisplay::Composite& lhs, const SlotDisplay::Composite& rhs) {
    return lhs.options == rhs.options;
}

bool operator==(const SlotDisplay& lhs, const SlotDisplay& rhs) { return lhs.payload_ == rhs.payload_; }

SlotDisplay::SlotDisplay() : payload_(Empty{}) {}

SlotDisplay::SlotDisplay(Payload payload) : payload_(std::move(payload)) {}

SlotDisplay::SlotDisplay(const SlotDisplay&) = default;

SlotDisplay::SlotDisplay(SlotDisplay&&) noexcept = default;

SlotDisplay& SlotDisplay::operator=(const SlotDisplay&) = default;

SlotDisplay& SlotDisplay::operator=(SlotDisplay&&) noexcept = default;

SlotDisplay::~SlotDisplay() = default;

std::int32_t SlotDisplay::type() const noexcept {
    if (std::holds_alternative<Item>(payload_)) {
        return kItemType;
    }
    if (std::holds_alternative<Composite>(payload_)) {
        return kCompositeType;
    }
    return kEmptyType;
}

void SlotDisplay::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, type());
    if (const auto* item = as<Item>()) {
        varInt::write(buffer, item->itemId);
    } else if (const auto* composite = as<Composite>()) {
        varInt::write(buffer, static_cast<std::int32_t>(composite->options.size()));
        for (const SlotDisplay& option : composite->options) {
            option.write(buffer);
        }
    }
    // An empty display writes nothing after its type.
}

std::optional<SlotDisplay> SlotDisplay::read(io::ByteBuffer& buffer) {
    const std::int32_t type = varInt::read(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    if (type == kEmptyType) {
        return SlotDisplay{Empty{}};
    }
    if (type == kItemType) {
        return SlotDisplay{Item{varInt::read(buffer)}};
    }
    if (type != kCompositeType) {
        buffer.markFailed();
        return std::nullopt;
    }

    Composite composite;
    const std::int32_t count = readCollectionSize(buffer);
    composite.options.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        auto option = read(buffer);
        if (!option) {
            return std::nullopt;
        }
        composite.options.push_back(std::move(*option));
    }
    return buffer.failed() ? std::nullopt : std::optional{SlotDisplay{std::move(composite)}};
}

void RecipeDisplay::write(io::ByteBuffer& buffer) const {
    if (grid) {
        varInt::write(buffer, grid->width);
        varInt::write(buffer, grid->height);
    }
    varInt::write(buffer, static_cast<std::int32_t>(ingredients.size()));
    for (const SlotDisplay& ingredient : ingredients) {
        ingredient.write(buffer);
    }
    result.write(buffer);
    craftingStation.write(buffer);
}

std::optional<RecipeDisplay> RecipeDisplay::read(io::ByteBuffer& buffer) {
    const std::int32_t type = varInt::read(buffer);
    if (buffer.failed() || (type != kShapelessType && type != kShapedType)) {
        buffer.markFailed();
        return std::nullopt;
    }

    RecipeDisplay display;
    if (type == kShapedType) {
        const std::int32_t width = varInt::read(buffer);
        display.grid = Grid{width, varInt::read(buffer)};
    }

    const std::int32_t count = readCollectionSize(buffer);
    display.ingredients.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        auto ingredient = SlotDisplay::read(buffer);
        if (!ingredient) {
            return std::nullopt;
        }
        display.ingredients.push_back(std::move(*ingredient));
    }

    auto result = SlotDisplay::read(buffer);
    auto station = SlotDisplay::read(buffer);
    if (!result || !station) {
        return std::nullopt;
    }
    display.result = std::move(*result);
    display.craftingStation = std::move(*station);
    return display;
}

void RecipeBookEntry::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, recipeId);
    varInt::write(buffer, display.type());
    display.write(buffer);
    writeBoolean(buffer, false);
    varInt::write(buffer, category);

    writeBoolean(buffer, !craftingRequirements.empty());
    if (!craftingRequirements.empty()) {
        varInt::write(buffer, static_cast<std::int32_t>(craftingRequirements.size()));
        for (const std::vector<std::int32_t>& ingredient : craftingRequirements) {
            // A holder set: the size plus one, because zero would mean a tag reference instead.
            varInt::write(buffer, static_cast<std::int32_t>(ingredient.size()) + 1);
            for (const std::int32_t item : ingredient) {
                varInt::write(buffer, item);
            }
        }
    }

    std::uint8_t flags = 0;
    if (notification) {
        flags |= 1;
    }
    if (highlight) {
        flags |= 2;
    }
    buffer.writeByte(flags);
}

std::optional<RecipeBookEntry> RecipeBookEntry::read(io::ByteBuffer& buffer) {
    RecipeBookEntry entry;
    entry.recipeId = varInt::read(buffer);
    auto display = RecipeDisplay::read(buffer);
    if (!display) {
        return std::nullopt;
    }
    entry.display = std::move(*display);

    if (readBoolean(buffer)) {
        // A group is never written here, so one arriving means the frame is not what it claims.
        buffer.markFailed();
        return std::nullopt;
    }
    entry.category = varInt::read(buffer);

    if (readBoolean(buffer)) {
        const std::int32_t count = readCollectionSize(buffer);
        entry.craftingRequirements.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
            const std::int32_t size = varInt::read(buffer) - 1;
            if (size < 0) {
                buffer.markFailed();
                return std::nullopt;
            }
            std::vector<std::int32_t> ingredient;
            ingredient.reserve(static_cast<std::size_t>(size));
            for (std::int32_t k = 0; k < size && !buffer.failed(); ++k) {
                ingredient.push_back(varInt::read(buffer));
            }
            entry.craftingRequirements.push_back(std::move(ingredient));
        }
    }

    const std::uint8_t flags = buffer.readByte();
    entry.notification = (flags & 1) != 0;
    entry.highlight = (flags & 2) != 0;
    return buffer.failed() ? std::nullopt : std::optional{std::move(entry)};
}

void RecipeBookAddPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(entries.size()));
    for (const RecipeBookEntry& entry : entries) {
        entry.write(buffer);
    }
    writeBoolean(buffer, replace);
}

std::optional<RecipeBookAddPacket> RecipeBookAddPacket::read(io::ByteBuffer& buffer) {
    RecipeBookAddPacket packet;
    const std::int32_t count = readCollectionSize(buffer, 4);
    packet.entries.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        auto entry = RecipeBookEntry::read(buffer);
        if (!entry) {
            return std::nullopt;
        }
        packet.entries.push_back(std::move(*entry));
    }
    packet.replace = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void RecipeBookRemovePacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(recipeIds.size()));
    for (const std::int32_t id : recipeIds) {
        varInt::write(buffer, id);
    }
}

std::optional<RecipeBookRemovePacket> RecipeBookRemovePacket::read(io::ByteBuffer& buffer) {
    RecipeBookRemovePacket packet;
    const std::int32_t count = readCollectionSize(buffer);
    packet.recipeIds.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        packet.recipeIds.push_back(varInt::read(buffer));
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void PlaceRecipePacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, containerId);
    varInt::write(buffer, recipeId);
    buffer.writeByte(useMaxItems ? 1 : 0);
}

std::optional<PlaceRecipePacket> PlaceRecipePacket::read(io::ByteBuffer& buffer) {
    PlaceRecipePacket packet;
    packet.containerId = varInt::read(buffer);
    packet.recipeId = varInt::read(buffer);
    packet.useMaxItems = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void PlaceGhostRecipePacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, containerId);
    display.write(buffer);
}

std::optional<PlaceGhostRecipePacket> PlaceGhostRecipePacket::read(io::ByteBuffer& buffer) {
    PlaceGhostRecipePacket packet;
    packet.containerId = varInt::read(buffer);
    std::optional<RecipeDisplay> display = RecipeDisplay::read(buffer);
    if (!display.has_value() || buffer.failed()) {
        return std::nullopt;
    }
    packet.display = std::move(*display);
    return packet;
}

void RecipeBookSettingsPacket::write(io::ByteBuffer& buffer) const {
    for (const Type& type : types) {
        buffer.writeByte(type.open ? 1 : 0);
        buffer.writeByte(type.filtering ? 1 : 0);
    }
}

std::optional<RecipeBookSettingsPacket> RecipeBookSettingsPacket::read(io::ByteBuffer& buffer) {
    RecipeBookSettingsPacket packet;
    for (Type& type : packet.types) {
        type.open = readBoolean(buffer);
        type.filtering = readBoolean(buffer);
    }
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void RecipeBookChangeSettingsPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, bookType);
    buffer.writeByte(open ? 1 : 0);
    buffer.writeByte(filtering ? 1 : 0);
}

std::optional<RecipeBookChangeSettingsPacket> RecipeBookChangeSettingsPacket::read(io::ByteBuffer& buffer) {
    RecipeBookChangeSettingsPacket packet;
    packet.bookType = varInt::read(buffer);
    packet.open = readBoolean(buffer);
    packet.filtering = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void RecipeBookSeenPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, recipeId); }

std::optional<RecipeBookSeenPacket> RecipeBookSeenPacket::read(io::ByteBuffer& buffer) {
    RecipeBookSeenPacket packet;
    packet.recipeId = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void UpdateRecipesPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(propertySets.size()));
    for (const ItemPropertySet& set : propertySets) {
        writeString(buffer, set.key);
        varInt::write(buffer, static_cast<std::int32_t>(set.itemIds.size()));
        for (const std::int32_t id : set.itemIds) {
            varInt::write(buffer, id);
        }
    }
    varInt::write(buffer, static_cast<std::int32_t>(stonecutter.size()));
    for (const StonecutterOption& option : stonecutter) {
        // parity: ByteBufCodecs.holderSet writes the size plus one, since zero would mean a tag.
        varInt::write(buffer, static_cast<std::int32_t>(option.inputItemIds.size()) + 1);
        for (const std::int32_t id : option.inputItemIds) {
            varInt::write(buffer, id);
        }
        varInt::write(buffer, SlotDisplay::kItemStackType);
        // parity: ItemStackTemplate.STREAM_CODEC.
        varInt::write(buffer, option.resultItemId);
        varInt::write(buffer, option.resultCount);
        varInt::write(buffer, 0);
        varInt::write(buffer, 0);
    }
}

std::optional<UpdateRecipesPacket> UpdateRecipesPacket::read(io::ByteBuffer& buffer) {
    UpdateRecipesPacket packet;
    const std::int32_t sets = readCollectionSize(buffer, 2);
    for (std::int32_t i = 0; i < sets && !buffer.failed(); ++i) {
        ItemPropertySet set{.key = readString(buffer), .itemIds = {}};
        const std::int32_t items = readCollectionSize(buffer);
        for (std::int32_t k = 0; k < items && !buffer.failed(); ++k) {
            set.itemIds.push_back(varInt::read(buffer));
        }
        packet.propertySets.push_back(std::move(set));
    }

    const std::int32_t count = readCollectionSize(buffer, 6);
    packet.stonecutter.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        StonecutterOption option;
        const std::int32_t inputs = varInt::read(buffer) - 1;
        if (inputs < 0) {
            buffer.markFailed();
            return std::nullopt;
        }
        option.inputItemIds.reserve(static_cast<std::size_t>(inputs));
        for (std::int32_t k = 0; k < inputs && !buffer.failed(); ++k) {
            option.inputItemIds.push_back(varInt::read(buffer));
        }
        static_cast<void>(varInt::read(buffer));
        option.resultItemId = varInt::read(buffer);
        option.resultCount = varInt::read(buffer);
        static_cast<void>(varInt::read(buffer));
        static_cast<void>(varInt::read(buffer));
        packet.stonecutter.push_back(std::move(option));
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::play
