#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/OlderComponents.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <vector>

namespace basalt::wire::multiversion::java::olderComponents {

namespace play = protocol::play;

std::optional<play::ItemComponent> template26_2(const play::ItemComponent& component, const ComposedTables& tables,
                                                std::int32_t farEnd) {
    constexpr std::int32_t kTooltipDisplay = 18;
    constexpr std::int32_t kPotDecorations = 76;
    constexpr std::int32_t kBrick = 1142;
    if (farEnd >= protocolVersions::kNative ||
        (component.id() != kTooltipDisplay && component.id() != kPotDecorations)) {
        return component;
    }
    io::ByteBuffer payload;
    component.writePayload(payload);
    io::ByteBuffer output;
    if (component.id() == kTooltipDisplay) {
        protocol::writeBoolean(output, protocol::readBoolean(payload));
        std::vector<std::int32_t> hidden;
        const std::int32_t count = protocol::readCollectionSize(payload);
        for (std::int32_t i = 0; i < count && !payload.failed(); ++i) {
            const std::int32_t id = tables.dataComponentType.map(protocol::varInt::read(payload));
            if (id != IdMap::kNone) {
                hidden.push_back(id);
            }
        }
        protocol::varInt::write(output, static_cast<std::int32_t>(hidden.size()));
        for (const std::int32_t id : hidden) {
            protocol::varInt::write(output, id);
        }
    } else {
        // Before 26.3 decorations were four item ids, not four optional ItemStackTemplates.
        protocol::varInt::write(output, 4);
        for (std::size_t side = 0; side < 4 && !payload.failed(); ++side) {
            std::int32_t item = kBrick;
            if (protocol::readBoolean(payload)) {
                item = protocol::varInt::read(payload);
                static_cast<void>(protocol::varInt::read(payload)); // Count.
                const std::int32_t added = protocol::readCollectionSize(payload, 2);
                const std::int32_t removed = protocol::readCollectionSize(payload);
                for (std::int32_t i = 0; i < added && !payload.failed(); ++i) {
                    const std::int32_t id = protocol::varInt::read(payload);
                    if (!play::ItemComponent::readPayload(payload, id, std::nullopt).has_value()) {
                        payload.markFailed();
                    }
                }
                for (std::int32_t i = 0; i < removed && !payload.failed(); ++i) {
                    static_cast<void>(protocol::varInt::read(payload));
                }
            }
            const std::int32_t mapped = tables.item.map(item);
            protocol::varInt::write(output, mapped == IdMap::kNone ? tables.item.map(kBrick) : mapped);
        }
    }
    if (payload.failed() || payload.readableBytes() != 0) {
        return std::nullopt;
    }
    return play::ItemComponent{component.id(), play::ItemComponent::Raw{output.toVector()}};
}

} // namespace basalt::wire::multiversion::java::olderComponents
