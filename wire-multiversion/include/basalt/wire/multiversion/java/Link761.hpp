#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.19.4 to 1.19.3.
///
/// 1.19.4 folded the packet that spawned a player into the one that spawns anything else, and split
/// being hurt out of the entity events into two packets of its own. Both go back here: a player
/// spawns with its own packet again, and a hit is an entity event and an animation.
class Link761 final : public RenumberingLink {
public:
    Link761(const VersionTables& tables, const ComposedTables& target) : RenumberingLink(tables), target_(&target) {}

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    const ComposedTables* target_;
};

} // namespace basalt::wire::multiversion::java
