#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.17.1 to 1.17, the last step of the range.
///
/// 1.17.1 gathered the entities being removed into one packet and gave every inventory update a
/// state id. 1.17 has neither: entities go one packet each, and the state id has nowhere to sit.
/// The contents of a window also count their slots in a short there rather than as a varint, and
/// what is on the cursor travels as a slot update of its own.
class Link755 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;
};

} // namespace basalt::wire::multiversion::java
