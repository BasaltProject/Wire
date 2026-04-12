#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/AdvancementPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

namespace basalt::wire::protocol::play {
namespace {

constexpr std::int32_t kBackgroundFlag = 1;
constexpr std::int32_t kToastFlag = 2;
constexpr std::int32_t kHiddenFlag = 4;

void writeOptionalString(io::ByteBuffer& buffer, const std::optional<std::string>& value) {
    writeBoolean(buffer, value.has_value());
    if (value.has_value()) {
        writeString(buffer, *value);
    }
}

[[nodiscard]] std::optional<std::string> readOptionalString(io::ByteBuffer& buffer) {
    if (!readBoolean(buffer)) {
        return std::nullopt;
    }
    return readString(buffer);
}

/// parity: `ItemStackTemplate.STREAM_CODEC`, the item before the count, then the patch.
void writeTemplate(io::ByteBuffer& buffer, const WireItemStack& stack) {
    varInt::write(buffer, stack.itemId);
    varInt::write(buffer, stack.count);
    varInt::write(buffer, static_cast<std::int32_t>(stack.components.size()));
    varInt::write(buffer, static_cast<std::int32_t>(stack.removedComponents.size()));
    for (const ItemComponent& component : stack.components) {
        varInt::write(buffer, component.id());
        component.writePayload(buffer);
    }
    for (const std::int32_t id : stack.removedComponents) {
        varInt::write(buffer, id);
    }
}

[[nodiscard]] WireItemStack readTemplate(io::ByteBuffer& buffer) {
    WireItemStack stack;
    stack.itemId = varInt::read(buffer);
    stack.count = varInt::read(buffer);
    const std::int32_t added = readCollectionSize(buffer, 2);
    const std::int32_t removed = readCollectionSize(buffer);
    for (std::int32_t i = 0; i < added && !buffer.failed(); ++i) {
        const std::int32_t id = varInt::read(buffer);
        auto component = ItemComponent::readPayload(buffer, id, std::nullopt);
        if (!component) {
            buffer.markFailed();
            return stack;
        }
        stack.components.push_back(std::move(*component));
    }
    for (std::int32_t i = 0; i < removed && !buffer.failed(); ++i) {
        stack.removedComponents.push_back(varInt::read(buffer));
    }
    return stack;
}

void writeDisplay(io::ByteBuffer& buffer, const AdvancementDisplayData& display) {
    writeAnonymousNbt(buffer, text::componentNbt::serialize(display.title));
    writeAnonymousNbt(buffer, text::componentNbt::serialize(display.description));
    writeTemplate(buffer, display.icon);
    varInt::write(buffer, display.frame);
    std::int32_t flags = 0;
    flags |= display.background.has_value() ? kBackgroundFlag : 0;
    flags |= display.showToast ? kToastFlag : 0;
    flags |= display.hidden ? kHiddenFlag : 0;
    buffer.writeInt(flags);
    if (display.background.has_value()) {
        writeString(buffer, *display.background);
    }
}

[[nodiscard]] AdvancementDisplayData readDisplay(io::ByteBuffer& buffer) {
    AdvancementDisplayData display;
    display.title = text::componentNbt::deserialize(readAnonymousNbt(buffer));
    display.description = text::componentNbt::deserialize(readAnonymousNbt(buffer));
    display.icon = readTemplate(buffer);
    display.frame = varInt::read(buffer);
    const std::int32_t flags = buffer.readInt();
    if ((flags & kBackgroundFlag) != 0) {
        display.background = readString(buffer);
    }
    display.showToast = (flags & kToastFlag) != 0;
    display.hidden = (flags & kHiddenFlag) != 0;
    return display;
}

} // namespace

void UpdateAdvancementsPacket::write(io::ByteBuffer& buffer) const {
    writeBoolean(buffer, reset);
    varInt::write(buffer, static_cast<std::int32_t>(added.size()));
    for (const AdvancementEntry& entry : added) {
        writeString(buffer, entry.id);
        writeOptionalString(buffer, entry.parent);
        writeBoolean(buffer, entry.display.has_value());
        if (entry.display.has_value()) {
            writeDisplay(buffer, *entry.display);
        }
        varInt::write(buffer, static_cast<std::int32_t>(entry.requirements.size()));
        for (const std::vector<std::string>& group : entry.requirements) {
            varInt::write(buffer, static_cast<std::int32_t>(group.size()));
            for (const std::string& criterion : group) {
                writeString(buffer, criterion);
            }
        }
        writeBoolean(buffer, entry.sendsTelemetryEvent);
        buffer.writeFloat(entry.x);
        buffer.writeFloat(entry.y);
    }
    varInt::write(buffer, static_cast<std::int32_t>(removed.size()));
    for (const std::string& id : removed) {
        writeString(buffer, id);
    }
    varInt::write(buffer, static_cast<std::int32_t>(progress.size()));
    for (const AdvancementProgressData& one : progress) {
        writeString(buffer, one.id);
        varInt::write(buffer, static_cast<std::int32_t>(one.criteria.size()));
        for (const auto& [criterion, obtained] : one.criteria) {
            writeString(buffer, criterion);
            writeBoolean(buffer, obtained.has_value());
            if (obtained.has_value()) {
                buffer.writeLong(*obtained);
            }
        }
    }
    writeBoolean(buffer, showAdvancements);
}

std::optional<UpdateAdvancementsPacket> UpdateAdvancementsPacket::read(io::ByteBuffer& buffer) {
    UpdateAdvancementsPacket packet;
    packet.reset = readBoolean(buffer);
    const std::int32_t added = readCollectionSize(buffer);
    for (std::int32_t i = 0; i < added && !buffer.failed(); ++i) {
        AdvancementEntry entry;
        entry.id = readString(buffer);
        entry.parent = readOptionalString(buffer);
        if (readBoolean(buffer)) {
            entry.display = readDisplay(buffer);
        }
        const std::int32_t groups = readCollectionSize(buffer);
        for (std::int32_t g = 0; g < groups && !buffer.failed(); ++g) {
            std::vector<std::string> group;
            const std::int32_t size = readCollectionSize(buffer);
            for (std::int32_t c = 0; c < size && !buffer.failed(); ++c) {
                group.push_back(readString(buffer));
            }
            entry.requirements.push_back(std::move(group));
        }
        entry.sendsTelemetryEvent = readBoolean(buffer);
        entry.x = buffer.readFloat();
        entry.y = buffer.readFloat();
        packet.added.push_back(std::move(entry));
    }
    const std::int32_t removed = readCollectionSize(buffer);
    for (std::int32_t i = 0; i < removed && !buffer.failed(); ++i) {
        packet.removed.push_back(readString(buffer));
    }
    const std::int32_t progress = readCollectionSize(buffer);
    for (std::int32_t i = 0; i < progress && !buffer.failed(); ++i) {
        AdvancementProgressData one;
        one.id = readString(buffer);
        const std::int32_t criteria = readCollectionSize(buffer);
        for (std::int32_t c = 0; c < criteria && !buffer.failed(); ++c) {
            std::string name = readString(buffer);
            std::optional<std::int64_t> obtained;
            if (readBoolean(buffer)) {
                obtained = buffer.readLong();
            }
            one.criteria.emplace_back(std::move(name), obtained);
        }
        packet.progress.push_back(std::move(one));
    }
    packet.showAdvancements = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void SelectAdvancementsTabPacket::write(io::ByteBuffer& buffer) const { writeOptionalString(buffer, tab); }

std::optional<SelectAdvancementsTabPacket> SelectAdvancementsTabPacket::read(io::ByteBuffer& buffer) {
    SelectAdvancementsTabPacket packet{.tab = readOptionalString(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void SeenAdvancementsPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(action));
    if (action == Action::OpenedTab) {
        writeString(buffer, tab.value_or(std::string{}));
    }
}

std::optional<SeenAdvancementsPacket> SeenAdvancementsPacket::read(io::ByteBuffer& buffer) {
    SeenAdvancementsPacket packet;
    const std::int32_t action = varInt::read(buffer);
    if (action == 0) {
        packet.action = Action::OpenedTab;
        packet.tab = readString(buffer);
    } else if (action == 1) {
        packet.action = Action::ClosedScreen;
    } else {
        return std::nullopt;
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::play
