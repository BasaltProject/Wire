#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// A 1.21.11 client on a 1.21.9 or 1.21.10 server: attributes in the dimension and biome, border times in ticks.
class Link773Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool configuration(MutablePacket& packet) const;
};

} // namespace basalt::wire::multiversion::java
