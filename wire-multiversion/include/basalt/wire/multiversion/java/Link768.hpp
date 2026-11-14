#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

class Link768 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// 1.21.2 folded steering and the shift key into one set of held inputs; these are the last ones sent.
    std::uint8_t movement_ = 0;
    bool sneaking_ = false;
};

} // namespace basalt::wire::multiversion::java
