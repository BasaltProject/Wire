#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <optional>
#include <set>

namespace basalt::wire::multiversion::java {

/// 26.3 to 26.2.
///
/// Arm swings leave the animate packet, particles carry their type first, light masks become byte
/// arrays, and a teleport is confirmed with the position it landed on. The brewing stand grows two
/// data slots a 26.2 client has nowhere to put.
class Link776 final : public RenumberingLink {
public:
    Link776(const VersionTables& tables, const ComposedTables& target) : RenumberingLink(tables), target_(&target) {}

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool configuration(MutablePacket& packet) const;

    /// Moves the particle type and its options from the front of the packet to its end.
    void particles(MutablePacket& packet) const;

    const ComposedTables* target_;
    /// Containers open as a brewing stand, whose third and fourth data slots are new.
    std::set<std::int32_t> brewingStands_;
    /// A teleport 26.2 confirmed by id alone, waiting for the move that says where it landed.
    std::optional<std::int32_t> pendingTeleport_;
};

} // namespace basalt::wire::multiversion::java
