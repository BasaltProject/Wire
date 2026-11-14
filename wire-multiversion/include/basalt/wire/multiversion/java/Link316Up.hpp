#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <unordered_map>

namespace basalt::wire::multiversion::java {

/// A 1.12 client on a 1.11.x server: sounds renumbered and illagers given the aggression flag 1.12 put first.
class Link316Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool entityData(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t type) const;

    std::unordered_map<std::int32_t, std::int32_t> types_;
};

} // namespace basalt::wire::multiversion::java
