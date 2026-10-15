#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <optional>
#include <set>

namespace basalt::wire::multiversion::java {

/// The brewing stand grows two data slots a 26.2 client has nowhere to put.
class Link776 final : public RenumberingLink {
public:
    Link776(const VersionTables& tables, const ComposedTables& target) : RenumberingLink(tables), target_(&target) {}

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool configuration(MutablePacket& packet) const;

    void particles(MutablePacket& packet) const;

    const ComposedTables* target_;
    std::set<std::int32_t> brewingStands_;
    std::optional<std::int32_t> pendingTeleport_;
};

} // namespace basalt::wire::multiversion::java
