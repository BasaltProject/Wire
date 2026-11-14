#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace basalt::wire::multiversion::java {

/// A 1.20.2 client on a 1.20 or 1.20.1 server: network NBT without a root name, players as entities, login order.
class Link763Up final : public RenumberingLink {
public:
    Link763Up(const VersionTables& upward, std::int32_t backendVersion);

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

    /// The server's numbering survives the links up to the tail, so particles and entity data kinds come in it.
    struct Numbering {
        std::array<std::int32_t, 9> particles{};
        std::vector<std::int32_t> serializers;
    };

private:
    Numbering numbering_;
};

} // namespace basalt::wire::multiversion::java
