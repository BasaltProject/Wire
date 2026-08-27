#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <map>
#include <string>

namespace basalt::wire::multiversion::java {

/// A 1.20.5 client on a 1.20.3 or 1.20.4 server: split registries, item stacks, dimension ids and attribute ids.
class Link765Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverboundForeign(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    std::map<std::string, std::int32_t> dimensionIds_;
    bool signedCommand_ = false;
};

} // namespace basalt::wire::multiversion::java
