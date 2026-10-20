#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <cstdint>

namespace basalt::wire::protocol::play {
namespace {

/// Hashes are fixed width, so unlike a real component patch this can always be skipped.
bool skipHashedStack(io::ByteBuffer& buffer) {
    if (!readBoolean(buffer)) {
        return false;
    }
    static_cast<void>(varInt::read(buffer));
    static_cast<void>(varInt::read(buffer));

    const std::int32_t added = readCollectionSize(buffer, 5);
    for (std::int32_t i = 0; i < added && !buffer.failed(); ++i) {
        static_cast<void>(varInt::read(buffer));
        static_cast<void>(buffer.readInt());
    }
    const std::int32_t removed = readCollectionSize(buffer);
    for (std::int32_t i = 0; i < removed && !buffer.failed(); ++i) {
        static_cast<void>(varInt::read(buffer));
    }
    return true;
}

std::vector<std::int32_t> readRemovedComponents(io::ByteBuffer& buffer, std::int32_t count) {
    std::vector<std::int32_t> removed;
    removed.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        removed.push_back(varInt::read(buffer));
    }
    return removed;
}

} // namespace

std::vector<std::pair<std::int32_t, std::int32_t>> WireItemStack::enchantments() const {
    for (const ItemComponent& component : components) {
        if (const auto* levels = component.as<ItemComponent::Enchantments>()) {
            if (!levels->stored) {
                return levels->levels;
            }
        }
    }
    return {};
}

std::int32_t WireItemStack::damage() const {
    for (const ItemComponent& component : components) {
        if (component.id() == dataComponent::kDamage) {
            if (const auto* number = component.as<ItemComponent::Number>()) {
                return number->value;
            }
        }
    }
    return 0;
}

std::optional<std::int32_t> WireItemStack::mapId() const {
    for (const ItemComponent& component : components) {
        if (component.id() == dataComponent::kMapId) {
            if (const auto* number = component.as<ItemComponent::Number>()) {
                return number->value;
            }
        }
    }
    return std::nullopt;
}

void WireItemStack::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, count);
    if (count <= 0) {
        return;
    }
    varInt::write(buffer, itemId);
    varInt::write(buffer, static_cast<std::int32_t>(components.size()));
    varInt::write(buffer, static_cast<std::int32_t>(removedComponents.size()));
    for (const ItemComponent& component : components) {
        varInt::write(buffer, component.id());
        component.writePayload(buffer);
    }
    for (const std::int32_t id : removedComponents) {
        varInt::write(buffer, id);
    }
}

void WireItemStack::writeOptional(io::ByteBuffer& buffer, const std::optional<WireItemStack>& stack) {
    if (stack) {
        stack->write(buffer);
    } else {
        varInt::write(buffer, 0);
    }
}

std::optional<WireItemStack> WireItemStack::read(io::ByteBuffer& buffer) {
    const std::int32_t count = varInt::read(buffer);
    if (buffer.failed() || count <= 0) {
        return std::nullopt;
    }

    WireItemStack stack;
    stack.count = count;
    stack.itemId = varInt::read(buffer);
    const std::int32_t added = readCollectionSize(buffer, 2);
    const std::int32_t removed = readCollectionSize(buffer);

    stack.components.reserve(static_cast<std::size_t>(added));
    for (std::int32_t i = 0; i < added && !buffer.failed(); ++i) {
        const std::int32_t id = varInt::read(buffer);
        auto component = ItemComponent::readPayload(buffer, id, std::nullopt);
        if (!component) {
            buffer.markFailed();
            return std::nullopt;
        }
        stack.components.push_back(std::move(*component));
    }
    stack.removedComponents = readRemovedComponents(buffer, removed);
    return buffer.failed() ? std::nullopt : std::optional{std::move(stack)};
}

std::optional<WireItemStack> WireItemStack::readDelimited(io::ByteBuffer& buffer) {
    const std::int32_t count = varInt::read(buffer);
    if (buffer.failed() || count <= 0) {
        return std::nullopt;
    }

    WireItemStack stack;
    stack.count = count;
    stack.itemId = varInt::read(buffer);
    const std::int32_t added = readCollectionSize(buffer, 3);
    const std::int32_t removed = readCollectionSize(buffer);

    stack.components.reserve(static_cast<std::size_t>(added));
    for (std::int32_t i = 0; i < added && !buffer.failed(); ++i) {
        const std::int32_t id = varInt::read(buffer);
        const std::int32_t length = varInt::read(buffer);
        if (length < 0 || static_cast<std::size_t>(length) > buffer.readableBytes()) {
            buffer.markFailed();
            return std::nullopt;
        }
        const std::size_t before = buffer.readableBytes();
        auto component = ItemComponent::readPayload(buffer, id, static_cast<std::size_t>(length));
        // Whatever was read, the next component starts where this one's length said it would.
        const std::size_t consumed = before - buffer.readableBytes();
        buffer.skip(static_cast<std::size_t>(length) - consumed);
        stack.components.push_back(component ? std::move(*component) : ItemComponent{id, ItemComponent::Raw{}});
    }
    stack.removedComponents = readRemovedComponents(buffer, removed);
    return buffer.failed() ? std::nullopt : std::optional{std::move(stack)};
}

void ContainerSetContentPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, containerId);
    varInt::write(buffer, stateId);
    varInt::write(buffer, static_cast<std::int32_t>(items.size()));
    for (const std::optional<WireItemStack>& item : items) {
        WireItemStack::writeOptional(buffer, item);
    }
    WireItemStack::writeOptional(buffer, carriedItem);
}

std::optional<ContainerSetContentPacket> ContainerSetContentPacket::read(io::ByteBuffer& buffer) {
    ContainerSetContentPacket packet;
    packet.containerId = varInt::read(buffer);
    packet.stateId = varInt::read(buffer);
    const std::int32_t size = readCollectionSize(buffer);
    packet.items.reserve(static_cast<std::size_t>(size));
    for (std::int32_t i = 0; i < size && !buffer.failed(); ++i) {
        packet.items.push_back(WireItemStack::read(buffer));
    }
    packet.carriedItem = WireItemStack::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ContainerSetSlotPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, containerId);
    varInt::write(buffer, stateId);
    buffer.writeShort(slot);
    WireItemStack::writeOptional(buffer, item);
}

std::optional<ContainerSetSlotPacket> ContainerSetSlotPacket::read(io::ByteBuffer& buffer) {
    ContainerSetSlotPacket packet;
    packet.containerId = varInt::read(buffer);
    packet.stateId = varInt::read(buffer);
    packet.slot = buffer.readShort();
    packet.item = WireItemStack::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void SetHeldSlotPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, slot); }

std::optional<SetHeldSlotPacket> SetHeldSlotPacket::read(io::ByteBuffer& buffer) {
    SetHeldSlotPacket packet{varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ServerboundSetCarriedItemPacket::write(io::ByteBuffer& buffer) const { buffer.writeShort(slot); }

std::optional<ServerboundSetCarriedItemPacket> ServerboundSetCarriedItemPacket::read(io::ByteBuffer& buffer) {
    ServerboundSetCarriedItemPacket packet{buffer.readShort()};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ServerboundContainerClickPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, containerId);
    varInt::write(buffer, stateId);
    buffer.writeShort(slot);
    buffer.writeByte(static_cast<std::uint8_t>(button));
    varInt::write(buffer, static_cast<std::int32_t>(mode));
    varInt::write(buffer, 0);
    buffer.writeByte(0);
}

std::optional<ServerboundContainerClickPacket> ServerboundContainerClickPacket::read(io::ByteBuffer& buffer) {
    ServerboundContainerClickPacket packet;
    packet.containerId = varInt::read(buffer);
    packet.stateId = varInt::read(buffer);
    packet.slot = buffer.readShort();
    packet.button = static_cast<std::int8_t>(buffer.readByte());

    const std::int32_t modeId = varInt::read(buffer);
    // parity: ByIdMap.OutOfBoundsStrategy.ZERO.
    packet.mode = (modeId >= 0 && modeId <= static_cast<std::int32_t>(Mode::PickupAll)) ? static_cast<Mode>(modeId)
                                                                                        : Mode::Pickup;

    const std::int32_t changed = readCollectionSize(buffer, 3);
    for (std::int32_t i = 0; i < changed && !buffer.failed(); ++i) {
        static_cast<void>(buffer.readShort());
        static_cast<void>(skipHashedStack(buffer));
    }
    packet.clientHasCarriedItem = skipHashedStack(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ServerboundSetCreativeModeSlotPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeShort(slot);
    WireItemStack::writeOptional(buffer, item);
}

std::optional<ServerboundSetCreativeModeSlotPacket> ServerboundSetCreativeModeSlotPacket::read(io::ByteBuffer& buffer) {
    ServerboundSetCreativeModeSlotPacket packet;
    packet.slot = buffer.readShort();
    packet.item = WireItemStack::readDelimited(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void OpenScreenPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, containerId);
    varInt::write(buffer, menuType);
    writeAnonymousNbt(buffer, text::componentNbt::serialize(title));
}

std::optional<OpenScreenPacket> OpenScreenPacket::read(io::ByteBuffer& buffer) {
    OpenScreenPacket packet;
    packet.containerId = varInt::read(buffer);
    packet.menuType = varInt::read(buffer);
    packet.title = text::componentNbt::deserialize(readAnonymousNbt(buffer));
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ContainerClosePacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, containerId); }

std::optional<ContainerClosePacket> ContainerClosePacket::read(io::ByteBuffer& buffer) {
    ContainerClosePacket packet{varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ContainerSetDataPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, containerId);
    buffer.writeShort(id);
    buffer.writeShort(value);
}

std::optional<ContainerSetDataPacket> ContainerSetDataPacket::read(io::ByteBuffer& buffer) {
    ContainerSetDataPacket packet;
    packet.containerId = varInt::read(buffer);
    packet.id = buffer.readShort();
    packet.value = buffer.readShort();
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void MerchantCost::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, itemId);
    varInt::write(buffer, count);
    // parity: DataComponentExactPredicate.
    varInt::write(buffer, 0);
}

MerchantCost MerchantCost::read(io::ByteBuffer& buffer) {
    MerchantCost cost;
    cost.itemId = varInt::read(buffer);
    cost.count = varInt::read(buffer);
    const std::int32_t exact = readCollectionSize(buffer, 2);
    for (std::int32_t i = 0; i < exact && !buffer.failed(); ++i) {
        static_cast<void>(varInt::read(buffer));
        static_cast<void>(varInt::read(buffer));
    }
    return cost;
}

void MerchantOffer::write(io::ByteBuffer& buffer) const {
    costA.write(buffer);
    result.write(buffer);
    writeBoolean(buffer, costB.has_value());
    if (costB) {
        costB->write(buffer);
    }
    writeBoolean(buffer, outOfStock);
    buffer.writeInt(uses);
    buffer.writeInt(maxUses);
    buffer.writeInt(experience);
    buffer.writeInt(specialPrice);
    buffer.writeFloat(priceMultiplier);
    buffer.writeInt(demand);
}

std::optional<MerchantOffer> MerchantOffer::read(io::ByteBuffer& buffer) {
    MerchantOffer offer;
    offer.costA = MerchantCost::read(buffer);
    auto result = WireItemStack::read(buffer);
    if (!result) {
        return std::nullopt;
    }
    offer.result = std::move(*result);
    if (readBoolean(buffer)) {
        offer.costB = MerchantCost::read(buffer);
    }
    offer.outOfStock = readBoolean(buffer);
    offer.uses = buffer.readInt();
    offer.maxUses = buffer.readInt();
    offer.experience = buffer.readInt();
    offer.specialPrice = buffer.readInt();
    offer.priceMultiplier = buffer.readFloat();
    offer.demand = buffer.readInt();
    return buffer.failed() ? std::nullopt : std::optional{std::move(offer)};
}

void MerchantOffersPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, containerId);
    varInt::write(buffer, static_cast<std::int32_t>(offers.size()));
    for (const MerchantOffer& offer : offers) {
        offer.write(buffer);
    }
    varInt::write(buffer, level);
    varInt::write(buffer, experience);
    writeBoolean(buffer, showProgress);
    writeBoolean(buffer, canRestock);
}

std::optional<MerchantOffersPacket> MerchantOffersPacket::read(io::ByteBuffer& buffer) {
    MerchantOffersPacket packet;
    packet.containerId = varInt::read(buffer);
    const std::int32_t count = readCollectionSize(buffer, 20);
    packet.offers.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        auto offer = MerchantOffer::read(buffer);
        if (!offer) {
            return std::nullopt;
        }
        packet.offers.push_back(std::move(*offer));
    }
    packet.level = varInt::read(buffer);
    packet.experience = varInt::read(buffer);
    packet.showProgress = readBoolean(buffer);
    packet.canRestock = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void SelectTradePacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, index); }

std::optional<SelectTradePacket> SelectTradePacket::read(io::ByteBuffer& buffer) {
    SelectTradePacket packet{varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ServerboundRenameItemPacket::write(io::ByteBuffer& buffer) const { writeString(buffer, name); }

std::optional<ServerboundRenameItemPacket> ServerboundRenameItemPacket::read(io::ByteBuffer& buffer) {
    ServerboundRenameItemPacket packet{readString(buffer, kMaxLength)};
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ServerboundEditBookPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, slot);
    varInt::write(buffer, static_cast<std::int32_t>(pages.size()));
    for (const std::string& page : pages) {
        writeString(buffer, page);
    }
    buffer.writeByte(title.has_value() ? 1 : 0);
    if (title.has_value()) {
        writeString(buffer, *title);
    }
}

std::optional<ServerboundEditBookPacket> ServerboundEditBookPacket::read(io::ByteBuffer& buffer) {
    ServerboundEditBookPacket packet;
    packet.slot = varInt::read(buffer);
    const std::int32_t count = varInt::read(buffer);
    if (count < 0 || static_cast<std::size_t>(count) > kMaxPages) {
        return std::nullopt;
    }
    packet.pages.reserve(static_cast<std::size_t>(count));
    for (std::int32_t page = 0; page < count && !buffer.failed(); ++page) {
        packet.pages.push_back(readString(buffer, kMaxPageLength));
    }
    if (buffer.readByte() != 0) {
        packet.title = readString(buffer, kMaxTitleLength);
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ServerboundSetBeaconPacket::write(io::ByteBuffer& buffer) const {
    const auto put = [&buffer](const std::optional<std::int32_t>& effect) {
        buffer.writeByte(effect.has_value() ? 1 : 0);
        if (effect.has_value()) {
            varInt::write(buffer, *effect);
        }
    };
    put(primary);
    put(secondary);
}

std::optional<ServerboundSetBeaconPacket> ServerboundSetBeaconPacket::read(io::ByteBuffer& buffer) {
    const auto take = [&buffer]() -> std::optional<std::int32_t> {
        if (buffer.readByte() == 0) {
            return std::nullopt;
        }
        return varInt::read(buffer);
    };
    ServerboundSetBeaconPacket packet;
    packet.primary = take();
    packet.secondary = take();
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ContainerButtonClickPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, containerId);
    varInt::write(buffer, buttonId);
}

std::optional<ContainerButtonClickPacket> ContainerButtonClickPacket::read(io::ByteBuffer& buffer) {
    ContainerButtonClickPacket packet;
    packet.containerId = varInt::read(buffer);
    packet.buttonId = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ContainerSlotStateChangedPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, slotId);
    varInt::write(buffer, containerId);
    buffer.writeByte(newState ? 1 : 0);
}

std::optional<ContainerSlotStateChangedPacket> ContainerSlotStateChangedPacket::read(io::ByteBuffer& buffer) {
    ContainerSlotStateChangedPacket packet;
    packet.slotId = varInt::read(buffer);
    packet.containerId = varInt::read(buffer);
    packet.newState = buffer.readByte() != 0;
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

} // namespace basalt::wire::protocol::play
