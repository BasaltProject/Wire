#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace basalt::wire::protocol::play {

/// Which of the option shapes a particle type carries. The registry says which type uses which.
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

/// The extra data a particle type carries after its id. parity: each ParticleOptions' own stream
/// codec, written straight after the type in ClientboundLevelParticlesPacket.
///
/// One case per *shape* rather than per particle: vanilla's twenty-three particles with options
/// share nine codecs between them, and which shape a type uses is the registry's business, not this
/// file's.
class ParticleOptions {
public:
    /// `block`, `block_marker`, `falling_dust`, `dust_pillar`, `block_crumble`: a block state id.
    struct BlockState {
        std::int32_t stateId = 0;

        friend bool operator==(const BlockState&, const BlockState&) = default;
    };

    /// `entity_effect`, `flash`, `tinted_leaves`: one packed colour, written as a plain int.
    struct Color {
        std::int32_t color = 0;

        friend bool operator==(const Color&, const Color&) = default;
    };

    /// `effect`, `instant_effect`: a colour and a power.
    struct Spell {
        std::int32_t color = 0;
        float power = 0.0F;

        friend bool operator==(const Spell&, const Spell&) = default;
    };

    /// `dust`: a colour and a scale.
    struct Dust {
        std::int32_t color = 0;
        float scale = 1.0F;

        friend bool operator==(const Dust&, const Dust&) = default;
    };

    /// `dust_color_transition`: the two ends of the fade, and a scale.
    struct DustTransition {
        std::int32_t fromColor = 0;
        std::int32_t toColor = 0;
        float scale = 1.0F;

        friend bool operator==(const DustTransition&, const DustTransition&) = default;
    };

    /// `item`: the stack it is drawn as.
    ///
    /// parity: ItemStackTemplate.STREAM_CODEC, which is *not* the ordinary stack codec: the item
    /// comes first and the count second, and there is no empty short form, because a template is
    /// never empty.
    struct Item {
        std::int32_t itemId = 0;
        std::int32_t count = 1;
        std::vector<ItemComponent> components;

        friend bool operator==(const Item&, const Item&) = default;
    };

    /// `trail`: where it is heading, its colour, and how long it takes.
    struct Trail {
        double targetX = 0.0;
        double targetY = 0.0;
        double targetZ = 0.0;
        std::int32_t color = 0;
        std::int32_t duration = 0;

        friend bool operator==(const Trail&, const Trail&) = default;
    };

    /// `sculk_charge` (a roll) and `dragon_breath` (a power): one float either way.
    struct Number {
        float value = 0.0F;

        friend bool operator==(const Number&, const Number&) = default;
    };

    /// `shriek`: how long the client waits before drawing it.
    struct Delay {
        std::int32_t ticks = 0;

        friend bool operator==(const Delay&, const Delay&) = default;
    };

    /// `geyser`, `geyser_plume`: how many water blocks fed it.
    struct Geyser {
        std::int32_t waterBlocks = 0;

        friend bool operator==(const Geyser&, const Geyser&) = default;
    };

    /// `geyser_base`, `geyser_poof`: the same, plus the impulse of the burst.
    struct GeyserBase {
        std::int32_t waterBlocks = 0;
        float burstImpulseBase = 0.0F;

        friend bool operator==(const GeyserBase&, const GeyserBase&) = default;
    };

    /// `vibration`: where the signal is going and when it lands.
    ///
    /// Only the block form of PositionSource is written, which is the one `/particle` itself can
    /// name; the entity form needs an entity the command has no way to give it.
    struct Vibration {
        BlockPos target;
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

    /// Reads the options of a type whose shape the caller knows, which is the proxy's side of this.
    [[nodiscard]] static std::optional<ParticleOptions> read(io::ByteBuffer& buffer, ParticleOptionShape shape);

    friend bool operator==(const ParticleOptions&, const ParticleOptions&) = default;

private:
    Payload payload_;
};

} // namespace basalt::wire::protocol::play
