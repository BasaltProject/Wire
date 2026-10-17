#pragma once

#include <basalt/wire/multiversion/ProtocolChain.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

/// The login packets of a client older than 1.20.2, which no link touches: written in its shape, read into ours.
class LoginRewriter final : public PacketContentRewriter {
public:
    explicit LoginRewriter(std::int32_t protocol) : protocol_(protocol) {}

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    std::int32_t protocol_;
};

} // namespace basalt::wire::multiversion::java
