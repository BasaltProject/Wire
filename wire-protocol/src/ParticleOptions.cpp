#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>

namespace basalt::wire::protocol::play {
namespace {

/// parity: the index of `minecraft:block` in the position_source_type registry.
constexpr std::int32_t kBlockPositionSource = 0;

} // namespace

void ParticleOptions::write(io::ByteBuffer& buffer) const {
    if (const auto* state = as<BlockState>()) {
        varInt::write(buffer, state->stateId);
    } else if (const auto* color = as<Color>()) {
        buffer.writeInt(color->color);
    } else if (const auto* spell = as<Spell>()) {
        buffer.writeInt(spell->color);
        buffer.writeFloat(spell->power);
    } else if (const auto* dust = as<Dust>()) {
        buffer.writeInt(dust->color);
        buffer.writeFloat(dust->scale);
    } else if (const auto* transition = as<DustTransition>()) {
        buffer.writeInt(transition->fromColor);
        buffer.writeInt(transition->toColor);
        buffer.writeFloat(transition->scale);
    } else if (const auto* item = as<Item>()) {
        varInt::write(buffer, item->itemId);
        varInt::write(buffer, item->count);
        varInt::write(buffer, static_cast<std::int32_t>(item->components.size()));
        // No removed components: a template patch only ever adds.
        varInt::write(buffer, 0);
        for (const ItemComponent& component : item->components) {
            varInt::write(buffer, component.id());
            component.writePayload(buffer);
        }
    } else if (const auto* trail = as<Trail>()) {
        buffer.writeDouble(trail->targetX);
        buffer.writeDouble(trail->targetY);
        buffer.writeDouble(trail->targetZ);
        buffer.writeInt(trail->color);
        varInt::write(buffer, trail->duration);
    } else if (const auto* number = as<Number>()) {
        buffer.writeFloat(number->value);
    } else if (const auto* delay = as<Delay>()) {
        varInt::write(buffer, delay->ticks);
    } else if (const auto* geyser = as<Geyser>()) {
        buffer.writeInt(geyser->waterBlocks);
    } else if (const auto* base = as<GeyserBase>()) {
        buffer.writeInt(base->waterBlocks);
        buffer.writeFloat(base->burstImpulseBase);
    } else if (const auto* vibration = as<Vibration>()) {
        varInt::write(buffer, kBlockPositionSource);
        buffer.writeLong(vibration->target.pack());
        varInt::write(buffer, vibration->arrivalTicks);
    }
}

std::optional<ParticleOptions> ParticleOptions::read(io::ByteBuffer& buffer, ParticleOptionShape shape) {
    switch (shape) {
    case ParticleOptionShape::None:
        return std::nullopt;
    case ParticleOptionShape::BlockState:
        return ParticleOptions{BlockState{varInt::read(buffer)}};
    case ParticleOptionShape::Color:
        return ParticleOptions{Color{buffer.readInt()}};
    case ParticleOptionShape::Spell: {
        const std::int32_t color = buffer.readInt();
        return ParticleOptions{Spell{color, buffer.readFloat()}};
    }
    case ParticleOptionShape::Dust: {
        const std::int32_t color = buffer.readInt();
        return ParticleOptions{Dust{color, buffer.readFloat()}};
    }
    case ParticleOptionShape::DustTransition: {
        const std::int32_t from = buffer.readInt();
        const std::int32_t to = buffer.readInt();
        return ParticleOptions{DustTransition{from, to, buffer.readFloat()}};
    }
    case ParticleOptionShape::Item: {
        Item item;
        item.itemId = varInt::read(buffer);
        item.count = varInt::read(buffer);
        const std::int32_t added = readCollectionSize(buffer, 2);
        const std::int32_t removed = readCollectionSize(buffer);
        item.components.reserve(static_cast<std::size_t>(added));
        for (std::int32_t i = 0; i < added && !buffer.failed(); ++i) {
            const std::int32_t id = varInt::read(buffer);
            // No length is on the wire here, so a component with no codec cannot be skipped over:
            // the rest of the packet would be read at the wrong offset.
            auto component = ItemComponent::readPayload(buffer, id, std::nullopt);
            if (!component) {
                buffer.markFailed();
                return std::nullopt;
            }
            item.components.push_back(std::move(*component));
        }
        for (std::int32_t i = 0; i < removed && !buffer.failed(); ++i) {
            static_cast<void>(varInt::read(buffer));
        }
        return buffer.failed() ? std::nullopt : std::optional{ParticleOptions{std::move(item)}};
    }
    case ParticleOptionShape::Trail: {
        Trail trail;
        trail.targetX = buffer.readDouble();
        trail.targetY = buffer.readDouble();
        trail.targetZ = buffer.readDouble();
        trail.color = buffer.readInt();
        trail.duration = varInt::read(buffer);
        return ParticleOptions{trail};
    }
    case ParticleOptionShape::Number:
        return ParticleOptions{Number{buffer.readFloat()}};
    case ParticleOptionShape::Delay:
        return ParticleOptions{Delay{varInt::read(buffer)}};
    case ParticleOptionShape::Geyser:
        return ParticleOptions{Geyser{buffer.readInt()}};
    case ParticleOptionShape::GeyserBase: {
        const std::int32_t blocks = buffer.readInt();
        return ParticleOptions{GeyserBase{blocks, buffer.readFloat()}};
    }
    case ParticleOptionShape::Vibration: {
        static_cast<void>(varInt::read(buffer));
        const BlockPos target = BlockPos::unpack(buffer.readLong());
        return ParticleOptions{Vibration{target, varInt::read(buffer)}};
    }
    }
    return std::nullopt;
}

} // namespace basalt::wire::protocol::play
