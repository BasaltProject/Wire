#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java::legacyStacks {
namespace {

using protocol::play::ItemComponent;
using protocol::play::WireItemStack;

constexpr std::int32_t kEmptyItem = -1;
constexpr std::int32_t kItemMask = 0xFFFF;
constexpr int kDamageShift = 16;

void writeTag(io::ByteBuffer& output, const std::optional<nbt::NbtCompound>& tag, std::int32_t farEnd) {
    if (!tag.has_value()) {
        output.writeByte(0);
        return;
    }
    if (farEnd < protocolVersions::kV1_20_2) {
        writeNamedNbt(output, nbt::NbtTag::ofCompound(*tag));
        return;
    }
    protocol::writeAnonymousNbt(output, nbt::NbtTag::ofCompound(*tag));
}

void writeFlagged(io::ByteBuffer& output, const std::optional<WireItemStack>& item, std::int32_t farEnd) {
    if (!item.has_value() || item->count <= 0) {
        protocol::writeBoolean(output, false);
        return;
    }
    protocol::writeBoolean(output, true);
    protocol::varInt::write(output, item->itemId);
    output.writeByte(static_cast<std::uint8_t>(item->count));
    writeTag(output, tagOf(*item), farEnd);
}

void writeShortId(io::ByteBuffer& output, const std::optional<WireItemStack>& item, std::int32_t farEnd) {
    if (!item.has_value() || item->count <= 0) {
        output.writeShort(static_cast<std::int16_t>(kEmptyItem));
        return;
    }
    output.writeShort(static_cast<std::int16_t>(item->itemId));
    output.writeByte(static_cast<std::uint8_t>(item->count));
    writeTag(output, tagOf(*item), farEnd);
}

void writeDamaged(io::ByteBuffer& output, const std::optional<WireItemStack>& item, std::int32_t farEnd) {
    if (!item.has_value() || item->count <= 0) {
        output.writeShort(static_cast<std::int16_t>(kEmptyItem));
        return;
    }
    output.writeShort(static_cast<std::int16_t>(item->itemId & kItemMask));
    output.writeByte(static_cast<std::uint8_t>(item->count));
    output.writeShort(static_cast<std::int16_t>(static_cast<std::uint32_t>(item->itemId) >> kDamageShift));
    writeTag(output, tagOf(*item), farEnd);
}

} // namespace

void write(io::ByteBuffer& output, const std::optional<WireItemStack>& item, std::int32_t farEnd) {
    if (farEnd >= protocolVersions::kV1_20_5) {
        WireItemStack::writeOptional(output, item);
    } else if (farEnd >= protocolVersions::kV1_13_2) {
        writeFlagged(output, item, farEnd);
    } else if (farEnd >= protocolVersions::kV1_13) {
        writeShortId(output, item, farEnd);
    } else {
        writeDamaged(output, item, farEnd);
    }
}

bool flaggedToShortId(io::ByteBuffer& input, io::ByteBuffer& output) {
    if (!protocol::readBoolean(input)) {
        output.writeShort(static_cast<std::int16_t>(kEmptyItem));
        return !input.failed();
    }
    const std::int32_t itemId = protocol::varInt::read(input);
    const std::uint8_t count = input.readByte();
    output.writeShort(static_cast<std::int16_t>(itemId));
    output.writeByte(count);
    return copyTag(input, output, Root::kNamed, Root::kNamed);
}

bool shortIdToFlagged(io::ByteBuffer& input, io::ByteBuffer& output) {
    const std::int16_t itemId = input.readShort();
    if (itemId == kEmptyItem) {
        protocol::writeBoolean(output, false);
        return !input.failed();
    }
    const std::uint8_t count = input.readByte();
    protocol::writeBoolean(output, true);
    protocol::varInt::write(output, itemId);
    output.writeByte(count);
    return copyTag(input, output, Root::kNamed, Root::kNamed);
}

bool copyFlagged(io::ByteBuffer& input, io::ByteBuffer& output, Root from, Root to) {
    const bool present = protocol::readBoolean(input);
    protocol::writeBoolean(output, present);
    if (!present) {
        return !input.failed();
    }
    protocol::varInt::write(output, protocol::varInt::read(input));
    output.writeByte(input.readByte());
    return copyTag(input, output, from, to);
}

bool copyNamed(io::ByteBuffer& input, io::ByteBuffer& output) {
    return copyFlagged(input, output, Root::kNamed, Root::kNamed);
}

bool copyTag(io::ByteBuffer& input, io::ByteBuffer& output, Root from, Root to) {
    if (from == Root::kNamed && to == Root::kNamed) {
        const std::optional<NamedNbt> tag = readNamedNbt(input);
        if (tag.has_value()) {
            output.writeBytes(tag->raw);
        }
        return tag.has_value() && !input.failed();
    }
    nbt::NbtTag tag;
    if (from == Root::kNamed) {
        std::optional<NamedNbt> named = readNamedNbt(input);
        if (!named.has_value()) {
            return false;
        }
        tag = std::move(named->tag);
    } else {
        tag = protocol::readAnonymousNbt(input);
    }
    if (to == Root::kNamed) {
        writeNamedNbt(output, tag);
    } else {
        protocol::writeAnonymousNbt(output, tag);
    }
    return !input.failed();
}

std::optional<nbt::NbtCompound> tagOf(const WireItemStack& item) {
    for (const ItemComponent& component : item.components) {
        const auto* custom = component.as<ItemComponent::CustomData>();
        if (custom != nullptr && component.id() == protocol::play::dataComponent::kCustomData && !custom->tag.empty()) {
            return custom->tag;
        }
    }
    return std::nullopt;
}

} // namespace basalt::wire::multiversion::java::legacyStacks
