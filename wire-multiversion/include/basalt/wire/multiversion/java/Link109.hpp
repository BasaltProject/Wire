#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// Below it a chunk carries no block entities, and a sign is a packet of its own.
class Link109 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] static bool separateSign(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
