#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.21.9 to 1.21.8. Three packets changed shape, all of them about where something is or how fast
/// it is going.
///
/// 1.21.9 replaced the three shorts a velocity used to be with a packed vector, and moved it in
/// front of the angles when an entity spawns. And the spawn point grew a dimension and a pitch,
/// which a 1.21.8 client has nowhere to put.
class Link772 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;
};

} // namespace basalt::wire::multiversion::java
