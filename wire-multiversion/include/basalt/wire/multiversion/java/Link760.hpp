#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

namespace basalt::wire::multiversion::java {

class Link760 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// Writes the one action a 1.19.1 client can read out of what this server sent.
    [[nodiscard]] static bool writeOneAction(MutablePacket& packet, const protocol::play::PlayerInfoUpdatePacket& info);
};

} // namespace basalt::wire::multiversion::java
