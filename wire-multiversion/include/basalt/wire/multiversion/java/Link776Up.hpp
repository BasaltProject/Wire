#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <optional>
#include <set>

namespace basalt::wire::multiversion::java {

class Link776Up final : public RenumberingLink {
public:
    explicit Link776Up(const VersionTables& upward);

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    struct Ids;

    [[nodiscard]] bool configuration(MutablePacket& packet) const;

    void particles(MutablePacket& packet) const;

    void moveEntity(MutablePacket& packet, std::int32_t kind) const;

    const Ids* ids_;
    std::optional<std::int32_t> pendingTeleport_;
};

} // namespace basalt::wire::multiversion::java
