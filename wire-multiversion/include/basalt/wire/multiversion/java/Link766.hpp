#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.21 to 1.20.5 and 1.20.6.
///
/// The three registries 1.21 invented (enchantments, paintings, jukebox songs) leave the
/// configuration, the rotation 1.21 added to using an item is filled in from the last movement, and
/// an attribute modifier goes back to being a uuid, made from its name so that it stays the same.
class Link766 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    static void unnameModifiers(MutablePacket& packet);

    /// The last rotation this client sent, which is what fills in the one 1.21 added to using an
    /// item. Per connection, like the link itself.
    float yaw_ = 0.0F;
    float pitch_ = 0.0F;
};

} // namespace basalt::wire::multiversion::java
