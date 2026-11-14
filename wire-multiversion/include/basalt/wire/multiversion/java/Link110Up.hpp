#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <string>
#include <unordered_set>

namespace basalt::wire::multiversion::java {

/// A 1.10 client on a 1.9.4 server: the no-gravity flag shifts entity data, sound pitch became a float.
class Link110Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    std::unordered_set<std::int32_t> potions_;
    std::string packHash_;
};

} // namespace basalt::wire::multiversion::java
