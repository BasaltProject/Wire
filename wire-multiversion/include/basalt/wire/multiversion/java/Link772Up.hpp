#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <vector>

namespace basalt::wire::multiversion::java {

/// A 1.21.9 client on a 1.21.7 or 1.21.8 server: velocities, spawns, explosions, the spawn position and the border centre.
class Link772Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    void border(MutablePacket& packet, bool initialize) const;

    std::vector<float> scales_;
    float scale_ = 1.0F;
    std::string dimension_ = "minecraft:overworld";
};

} // namespace basalt::wire::multiversion::java
