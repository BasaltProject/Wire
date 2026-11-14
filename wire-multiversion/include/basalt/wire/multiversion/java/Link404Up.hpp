#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

/// A 1.14 client on a 1.13.2 server: light apart from the chunk, a view centre, packed positions, menus, trades.
class Link404Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool chunk(MutablePacket& packet);
    [[nodiscard]] bool openWindow(MutablePacket& packet);
    [[nodiscard]] bool customPayload(MutablePacket& packet);
    void centreOn(double x, double z);
    void announceCentre(MutablePacket& packet);

    bool hasSkyLight_ = true;
    bool centred_ = false;
    std::int32_t wantedX_ = 0;
    std::int32_t wantedZ_ = 0;
    std::int32_t sentX_ = 0;
    std::int32_t sentZ_ = 0;
    bool sent_ = false;
};

} // namespace basalt::wire::multiversion::java
