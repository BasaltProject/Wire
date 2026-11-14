#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

/// A 1.20.3 client on a 1.20.2 server: text as NBT, scores, resource packs with ids and the chunk loading event.
class Link764Up final : public RenumberingLink {
public:
    Link764Up(const VersionTables& upward, std::int32_t backendVersion);

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// The explosion particles in the numbering of the server, which is what the chain renumbers from.
    std::int32_t smallExplosion_;
    std::int32_t largeExplosion_;
};

} // namespace basalt::wire::multiversion::java
