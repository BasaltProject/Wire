#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace basalt::wire::multiversion::java {

/// A client below 1.12.2 echoes a keep-alive as a varint; the long it stood for is kept to answer with.
class Link338 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    std::array<std::int64_t, 8> keepAlives_{};
    std::size_t keepAliveNext_ = 0;
};

} // namespace basalt::wire::multiversion::java
