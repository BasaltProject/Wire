#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

/// 1.17 to 1.16.4 and 1.16.5, where the world stops fitting.
///
/// 1.16 has sixteen sections from 0 to 255 and no way to say otherwise, so the chunk, the light and
/// every block update are cut to that band and the masks become a single number.
///
/// The five title packets 1.17 split out come back together as the one packet 1.16 has.
class Link754 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// One of the three halves of a title that carry text, as the one packet 1.16 has for all of it.
    [[nodiscard]] static bool asTitle(MutablePacket& packet, std::int32_t action);

    /// Keeps the sixteen sections a 1.16 client has room for, and says which of them are there in
    /// the one number it reads.
    [[nodiscard]] static bool cutChunk(MutablePacket& packet);

    /// The same cut for the light, whose masks are a number here and whose arrays follow in order.
    ///
    /// 1.16 lights one section above and one below the world, which is why the band is eighteen wide
    /// rather than sixteen.
    [[nodiscard]] static bool cutLight(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
