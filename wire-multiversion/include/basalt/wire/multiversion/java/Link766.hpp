#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

class Link766 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    static void unnameModifiers(MutablePacket& packet);

    /// The last rotation this client sent, which is what fills in the one 1.21 added to using an item.
    float yaw_ = 0.0F;
    float pitch_ = 0.0F;
};

} // namespace basalt::wire::multiversion::java
