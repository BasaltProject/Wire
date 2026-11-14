#pragma once

#include <basalt/wire/multiversion/ProtocolChain.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

/// The links hand 1.13 and 1.13.1 their stacks as 1.13.2 has them; this reshapes them at the client's edge.
class Stacks113 final : public PacketContentRewriter {
public:
    Stacks113(std::int32_t protocol, PacketContentRewriter& next);

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    PacketContentRewriter* next_;
    std::int32_t creativeSlotId_;
    std::int32_t editBookId_;
};

} // namespace basalt::wire::multiversion::java
