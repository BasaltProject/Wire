#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

class Link761 final : public RenumberingLink {
public:
    Link761(const VersionTables& tables, const ComposedTables& target) : RenumberingLink(tables), target_(&target) {}

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    const ComposedTables* target_;
};

} // namespace basalt::wire::multiversion::java
