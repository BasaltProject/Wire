#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <span>

namespace basalt::wire::multiversion::java {

/// 1.19 to 1.18.2, the version before the chat was signed and before every entity spawned the same
/// way.
///
/// Three things go back: anything alive spawns with its own packet again and with its angles the
/// other way round, a sound loses the seed 1.19 gave it, and a system message becomes an ordinary
/// chat with a position byte.
class Link758 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    /// One field of the head of a packet this link copies across without changing it.
    enum class Field : std::uint8_t { Int, Long, Byte, VarInt, String, Nbt, StringArray };

private:
    /// Cuts the death location off the end, which is what 1.19 added to both of these.
    ///
    /// The head of the packet is copied byte for byte: none of it changes here, and by this point
    /// the shape is the one the links above built rather than anything this server writes.
    static void dropDeathLocation(MutablePacket& packet, std::span<const Field> headFields);
};

} // namespace basalt::wire::multiversion::java
