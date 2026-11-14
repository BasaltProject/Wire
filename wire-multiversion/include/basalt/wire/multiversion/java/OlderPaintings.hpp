#pragma once

#include <basalt/wire/multiversion/ProtocolChain.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::wire::multiversion::java {

/// Before 1.19 a painting spawned with its motive, which now comes after the spawn in the entity data.
class OlderPaintings {
public:
    explicit OlderPaintings(std::int32_t farEnd) : farEnd_(farEnd) {}

    /// Numbers the synced painting variants by the motives of 1.18, from a configuration registry packet.
    void learn(const std::vector<std::uint8_t>& registryData);

    /// Keeps a painting's spawn back until its variant arrives.
    void hold(const MutablePacket& spawn);

    [[nodiscard]] bool holding() const noexcept { return held_.has_value(); }

    /// The motive in this packet when it is the held painting's entity data.
    [[nodiscard]] std::optional<std::int32_t> motiveIn(const MutablePacket& packet) const;

    /// Turns the packet into the held spawn, carrying the motive in its entity type.
    void spawn(MutablePacket& packet, std::int32_t motive);

    /// Lets the held spawn go with the first variant, unless this packet removes the painting.
    [[nodiscard]] std::optional<MutablePacket::Continued> release(const MutablePacket& next);

    /// parity: `Motive` before 1.13, which a spawn named by its title.
    [[nodiscard]] static std::string_view title(std::int32_t motive) noexcept;

private:
    [[nodiscard]] std::int32_t firstMotive() const noexcept { return motives_.empty() ? 0 : motives_.front(); }

    std::int32_t farEnd_;
    std::vector<std::int32_t> motives_;
    std::optional<std::vector<std::uint8_t>> held_;
    std::int32_t heldId_ = 0;
};

} // namespace basalt::wire::multiversion::java
