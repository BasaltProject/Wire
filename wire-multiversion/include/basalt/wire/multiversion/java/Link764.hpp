#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.20.3 to 1.20.2, where text goes back to being a string of json: every packet carrying text is
/// written again with the same component serialized the old way.
///
/// The ones covered are the ones this server sends. A boss bar or a scoreboard is not renamed here,
/// which is why neither reaches a client this old at all rather than reaching it broken.
class Link764 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;
};

} // namespace basalt::wire::multiversion::java
