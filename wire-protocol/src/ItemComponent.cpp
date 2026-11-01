#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <cstdint>

namespace basalt::wire::protocol::play {
namespace {

// Native DataComponentType ids. These patches also occur without lengths in ItemStackTemplate.
constexpr std::int32_t kTooltipDisplay = 18;
constexpr std::int32_t kBannerPatterns = 74;
constexpr std::int32_t kPotDecorations = 76;

void skipPotDecorations(io::ByteBuffer& buffer, std::size_t depth = 0) {
    if (depth >= 64) {
        buffer.markFailed();
        return;
    }
    for (std::size_t side = 0; side < 4 && !buffer.failed(); ++side) {
        if (!readBoolean(buffer)) {
            continue;
        }
        static_cast<void>(varInt::read(buffer)); // Item and count of the optional template.
        static_cast<void>(varInt::read(buffer));
        const std::int32_t added = readCollectionSize(buffer, 2);
        const std::int32_t removed = readCollectionSize(buffer);
        for (std::int32_t i = 0; i < added && !buffer.failed(); ++i) {
            const std::int32_t id = varInt::read(buffer);
            if (id == kPotDecorations) {
                skipPotDecorations(buffer, depth + 1);
            } else if (!ItemComponent::readPayload(buffer, id, std::nullopt).has_value()) {
                buffer.markFailed();
            }
        }
        for (std::int32_t i = 0; i < removed && !buffer.failed(); ++i) {
            static_cast<void>(varInt::read(buffer));
        }
    }
}

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
            // parity: Enchantment.STREAM_CODEC is holderRegistry, which writes the plain id. Adding.
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
    case kMapId:
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
    case kTooltipDisplay:
    case kBannerPatterns:
    case kPotDecorations: {
        if (length.has_value()) {
            return ItemComponent{id, Raw{buffer.readBytes(*length)}};
        }
        const auto payload = buffer.readable();
        if (id == kPotDecorations) {
            skipPotDecorations(buffer);
        } else {
            if (id == kTooltipDisplay) {
                static_cast<void>(readBoolean(buffer));
            }
            const std::int32_t count = readCollectionSize(buffer, id == kBannerPatterns ? 2 : 1);
            for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
                const std::int32_t entry = varInt::read(buffer);
                if (id == kBannerPatterns) {
                    if (entry == 0) { // Inline BannerPattern: asset id and translation key.
                        static_cast<void>(readString(buffer));
                        static_cast<void>(readString(buffer));
                    }
                    static_cast<void>(varInt::read(buffer)); // Dye color.
                }
            }
        }
        if (buffer.failed()) {
            return std::nullopt;
        }
        const auto bytes = payload.first(payload.size() - buffer.readableBytes());
        return ItemComponent{id, Raw{{bytes.begin(), bytes.end()}}};
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
