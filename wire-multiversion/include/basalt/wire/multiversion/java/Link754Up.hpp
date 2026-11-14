#pragma once

#include <basalt/wire/multiversion/java/LegacyClicks.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// A 1.17 client on a 1.16.5 server: the world is declared 256 tall and several packets are split or reshaped.
class Link754Up final : public RenumberingLink {
public:
    Link754Up(const VersionTables& upward, std::int32_t backendVersion)
        : RenumberingLink(upward), clicks_(backendVersion) {}

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverboundForeign(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    LegacyClicks clicks_;
};

} // namespace basalt::wire::multiversion::java
