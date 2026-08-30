#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.21.4 to 1.21.3. Two fields, both small.
///
/// The slot in the hand became a variable-length number in 1.21.4 and used to be a single byte, and
/// a particle gained the flag that shows it however far away it is.
class Link768 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;
};

} // namespace basalt::wire::multiversion::java
