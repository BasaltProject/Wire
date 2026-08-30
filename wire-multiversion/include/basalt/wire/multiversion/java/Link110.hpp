#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.10.2 to 1.9.4, where the pitch of a sound is one byte.
///
/// 1.10 made it a float. Below that it is the same number times sixty three, rounded, which is
/// exactly what the client divides it back by.
class Link110 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;
};

} // namespace basalt::wire::multiversion::java
