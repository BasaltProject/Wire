#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 26.1 to 1.21.11.
///
/// The clock becomes the overworld's alone, attacking goes back inside interacting, and a wolf's
/// sounds are flattened to the adult's, which is the only set 1.21.11 has.
class Link774 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// A wolf's sounds, as the one flat set 1.21.11 reads.
    static void flattenWolfSounds(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
