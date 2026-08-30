#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.11 to 1.10.2, where a mob is one byte and there is no line above the hotbar.
///
/// 1.11 gave the kind of a mob a varint, added the action bar to the title packet and started saying
/// how many items were picked up. A client below it has one byte for the mob, four actions where
/// 1.11 has six, and no field for the count.
class Link210 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;
};

} // namespace basalt::wire::multiversion::java
