#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

class Link754 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] static bool asTitle(MutablePacket& packet, std::int32_t action);

    [[nodiscard]] static bool cutChunk(MutablePacket& packet);

    /// The same cut for the light, whose masks are a number here and whose arrays follow in order.
    [[nodiscard]] static bool cutLight(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
