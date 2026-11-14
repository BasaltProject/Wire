#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <unordered_map>

namespace basalt::wire::multiversion::java {

/// A 1.11 client on a 1.10 server: mob variants become their own types and names become namespaced ids.
class Link210Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool entities(MutablePacket& packet);

    std::unordered_map<std::int32_t, std::int32_t> types_;
};

} // namespace basalt::wire::multiversion::java
