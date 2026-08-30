#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.9.2 to 1.9, where the dimension fits in one byte.
///
/// The only difference between the two: 1.9.2 widened it to four so that a mod could have more of
/// them, and the three vanilla has fit either way.
class Link107 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;
};

} // namespace basalt::wire::multiversion::java
