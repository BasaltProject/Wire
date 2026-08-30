#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <vector>

namespace basalt::wire::multiversion::java {

/// 1.13 to 1.12.2, the flattening.
///
/// A block is an id and four bits of metadata below 1.13, and an item an id and a damage. The
/// numbers are the tables' work; the shape of a stack is this one's.
///
/// Dropped rather than sent wrong: particles, statistics and maps, whose payloads carry no meaning
/// across.
class Link340 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// The biomes of a chunk, which are one byte each below 1.13.
    ///
    /// They are the tail of the data buffer, so the sections do not have to be walked to find them:
    /// a full chunk ends with two hundred and fifty six of them and nothing else.
    static void narrowBiomes(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
