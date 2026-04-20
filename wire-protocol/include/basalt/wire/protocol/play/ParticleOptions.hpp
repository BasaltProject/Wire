#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace basalt::wire::protocol::play {

enum class ParticleOptionShape : std::uint8_t {
    None,
    BlockState,
    Color,
    Spell,
    Dust,
    DustTransition,
    Item,
    Trail,
    Number,
    Delay,
    Geyser,
    GeyserBase,
    Vibration,
};

/// parity: each ParticleOptions' own stream.
class ParticleOptions {
public:
    struct BlockState {
        std::int32_t stateId = 0;

        friend bool operator==(const BlockState&, const BlockState&) = default;
    };

    struct Color {
        std::int32_t color = 0;

        friend bool operator==(const Color&, const Color&) = default;
    };

    struct Spell {
        std::int32_t color = 0;
        float power = 0.0F;

        friend bool operator==(const Spell&, const Spell&) = default;
    };

    struct Dust {
        std::int32_t color = 0;
        float scale = 1.0F;

        friend bool operator==(const Dust&, const Dust&) = default;
    };

    struct DustTransition {
        std::int32_t fromColor = 0;
        std::int32_t toColor = 0;
        float scale = 1.0F;

        friend bool operator==(const DustTransition&, const DustTransition&) = default;
    };

    /// parity: ItemStackTemplate.STREAM_CODEC.
    struct Item {
        std::int32_t itemId = 0;
        std::int32_t count = 1;
        std::vector<ItemComponent> components;

        friend bool operator==(const Item&, const Item&) = default;
    };

    struct Trail {
        double targetX = 0.0;
        double targetY = 0.0;
        double targetZ = 0.0;
        std::int32_t color = 0;
        std::int32_t duration = 0;

        friend bool operator==(const Trail&, const Trail&) = default;
    };

    struct Number {
        float value = 0.0F;

        friend bool operator==(const Number&, const Number&) = default;
    };

    struct Delay {
        std::int32_t ticks = 0;

        friend bool operator==(const Delay&, const Delay&) = default;
    };

    struct Geyser {
        std::int32_t waterBlocks = 0;

        friend bool operator==(const Geyser&, const Geyser&) = default;
    };

    struct GeyserBase {
        std::int32_t waterBlocks = 0;
        float burstImpulseBase = 0.0F;

        friend bool operator==(const GeyserBase&, const GeyserBase&) = default;
    };

    struct Vibration {
        /// parity: `EntityPositionSource`, the target of a warden's or an allay's vibration.
        struct EntityTarget {
            std::int32_t entityId = 0;
            float yOffset = 0.0F;

            friend bool operator==(const EntityTarget&, const EntityTarget&) = default;
        };

        /// parity: `PositionSource`, a block or an entity with a y offset.
        std::variant<BlockPos, EntityTarget> target;
        std::int32_t arrivalTicks = 0;

        friend bool operator==(const Vibration&, const Vibration&) = default;
    };

    using Payload = std::variant<BlockState, Color, Spell, Dust, DustTransition, Item, Trail, Number, Delay, Geyser,
                                 GeyserBase, Vibration>;

    explicit ParticleOptions(Payload payload) : payload_(std::move(payload)) {}

    [[nodiscard]] const Payload& payload() const noexcept { return payload_; }

    template <typename T>
    [[nodiscard]] const T* as() const noexcept {
        return std::get_if<T>(&payload_);
    }

    void write(io::ByteBuffer& buffer) const;

    [[nodiscard]] static std::optional<ParticleOptions> read(io::ByteBuffer& buffer, ParticleOptionShape shape);

    friend bool operator==(const ParticleOptions&, const ParticleOptions&) = default;

private:
    Payload payload_;
};

} // namespace basalt::wire::protocol::play
