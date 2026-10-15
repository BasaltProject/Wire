#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java::legacyStacks {
namespace {

using protocol::play::ItemComponent;
using protocol::play::WireItemStack;

constexpr std::int32_t kEmptyItem = -1;
constexpr std::int32_t kItemMask = 0xFFFF;
constexpr int kDamageShift = 16;

void writeTag(io::ByteBuffer& output, const std::optional<nbt::NbtCompound>& tag) {
    if (!tag.has_value()) {
        output.writeByte(0);
        return;
    }
    protocol::writeAnonymousNbt(output, nbt::NbtTag::ofCompound(*tag));
}

void writeTagged(io::ByteBuffer& output, const std::optional<WireItemStack>& item) {
    if (!item.has_value() || item->count <= 0) {
        protocol::writeBoolean(output, false);
        return;
    }
    protocol::writeBoolean(output, true);
    protocol::varInt::write(output, item->itemId);
    output.writeByte(static_cast<std::uint8_t>(item->count));
    writeTag(output, tagOf(*item));
}

void writeDamaged(io::ByteBuffer& output, const std::optional<WireItemStack>& item) {
    if (!item.has_value() || item->count <= 0) {
        output.writeShort(static_cast<std::int16_t>(kEmptyItem));
        return;
    }
    output.writeShort(static_cast<std::int16_t>(item->itemId & kItemMask));
    output.writeByte(static_cast<std::uint8_t>(item->count));
    output.writeShort(static_cast<std::int16_t>(static_cast<std::uint32_t>(item->itemId) >> kDamageShift));
    writeTag(output, tagOf(*item));
}

} // namespace

void write(io::ByteBuffer& output, const std::optional<WireItemStack>& item, std::int32_t farEnd) {
    if (farEnd >= protocolVersions::kV1_20_5) {
        WireItemStack::writeOptional(output, item);
    } else if (farEnd >= protocolVersions::kV1_13) {
        writeTagged(output, item);
    } else {
        writeDamaged(output, item);
    }
}

std::optional<nbt::NbtCompound> tagOf(const WireItemStack& item) {
    std::optional<nbt::NbtCompound> tag;
    for (const ItemComponent& component : item.components) {
        if (component.id() == protocol::play::dataComponent::kDamage) {
            if (const auto* number = component.as<ItemComponent::Number>()) {
                if (!tag.has_value()) {
                    tag.emplace();
                }
                tag->putInt("Damage", number->value);
            }
        } else if (component.id() == protocol::play::dataComponent::kCustomName) {
            if (const auto* text = component.as<ItemComponent::Text>()) {
                if (!tag.has_value()) {
                    tag.emplace();
                }
                // A name travelled as a string of json before it was a component of its own.
                nbt::NbtCompound display;
                display.putString("Name", common::writeJson(text::componentJson::serialize(text->text)));
                tag->set("display", nbt::NbtTag::ofCompound(std::move(display)));
            }
        }
    }
    return tag;
}

} // namespace basalt::wire::multiversion::java::legacyStacks
