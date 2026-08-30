#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

namespace basalt::wire::protocol::play {
namespace {

std::vector<std::pair<std::int32_t, std::int32_t>> readLevels(io::ByteBuffer& buffer) {
    const std::int32_t count = readCollectionSize(buffer, 2);
    std::vector<std::pair<std::int32_t, std::int32_t>> levels;
    levels.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        const std::int32_t enchantment = varInt::read(buffer);
        levels.emplace_back(enchantment, varInt::read(buffer));
    }
    return levels;
}

} // namespace

void ItemComponent::writePayload(io::ByteBuffer& buffer) const {
    if (const auto* enchantments = as<Enchantments>()) {
        varInt::write(buffer, static_cast<std::int32_t>(enchantments->levels.size()));
        for (const auto& [enchantment, level] : enchantments->levels) {
            // parity: Enchantment.STREAM_CODEC is holderRegistry, which writes the plain id. Adding
            // one landed every enchantment on the next entry of the registry.
            varInt::write(buffer, enchantment);
            varInt::write(buffer, level);
        }
    } else if (const auto* number = as<Number>()) {
        varInt::write(buffer, number->value);
    } else if (const auto* color = as<Color>()) {
        buffer.writeInt(color->rgb);
    } else if (const auto* text = as<Text>()) {
        writeAnonymousNbt(buffer, wire::text::componentNbt::serialize(text->text));
    } else if (const auto* lore = as<Lore>()) {
        varInt::write(buffer, static_cast<std::int32_t>(lore->lines.size()));
        for (const wire::text::Component& line : lore->lines) {
            writeAnonymousNbt(buffer, wire::text::componentNbt::serialize(line));
        }
    } else if (const auto* custom = as<CustomData>()) {
        writeAnonymousNbt(buffer, nbt::NbtTag::ofCompound(custom->tag));
    } else if (const auto* flag = as<Flag>()) {
        writeBoolean(buffer, flag->value);
    } else if (const auto* raw = as<Raw>()) {
        buffer.writeBytes(raw->payload);
    }
    // A unit component writes nothing at all: being there is the whole message.
}

std::optional<ItemComponent> ItemComponent::readPayload(io::ByteBuffer& buffer, std::int32_t id,
                                                        std::optional<std::size_t> length) {
    using namespace dataComponent;

    switch (id) {
    case kEnchantments:
    case kStoredEnchantments:
        return ItemComponent{id, Enchantments{readLevels(buffer), id == kStoredEnchantments}};
    case kMaxStackSize:
    case kMaxDamage:
    case kDamage:
    case kRarity:
    case kRepairCost:
        return ItemComponent{id, Number{varInt::read(buffer)}};
    case kUnbreakable:
    case kCreativeSlotLock:
        return ItemComponent{id, Unit{}};
    case kEnchantmentGlintOverride:
        return ItemComponent{id, Flag{readBoolean(buffer)}};
    case kDyedColor:
        return ItemComponent{id, Color{buffer.readInt()}};
    case kCustomName:
    case kItemName:
        return ItemComponent{id, Text{wire::text::componentNbt::deserialize(readAnonymousNbt(buffer))}};
    case kLore: {
        Lore lore;
        const std::int32_t count = readCollectionSize(buffer, 2);
        lore.lines.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
            lore.lines.push_back(wire::text::componentNbt::deserialize(readAnonymousNbt(buffer)));
        }
        return ItemComponent{id, std::move(lore)};
    }
    case kCustomData: {
        const nbt::NbtTag tag = readAnonymousNbt(buffer);
        const auto* compound = tag.get<nbt::NbtCompound>();
        return ItemComponent{id, CustomData{compound != nullptr ? *compound : nbt::NbtCompound{}}};
    }
    default:
        break;
    }

    if (!length) {
        return std::nullopt;
    }
    return ItemComponent{id, Raw{buffer.readBytes(*length)}};
}

} // namespace basalt::wire::protocol::play
