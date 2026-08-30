#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.9.4 to 1.9.2, where a chunk does not carry its block entities.
///
/// 1.9.4 added the list at the end of the chunk. Below it a sign or a chest sends its own packet,
/// which this server does anyway, so the list simply goes.
class Link109 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;
};

} // namespace basalt::wire::multiversion::java
