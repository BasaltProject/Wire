#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

class Link765 final : public RenumberingLink {
public:
    Link765(const VersionTables& tables, const ComposedTables& target) : RenumberingLink(tables), target_(&target) {}

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    void nameAttributes(MutablePacket& packet) const;

    const ComposedTables* target_;
};

} // namespace basalt::wire::multiversion::java
