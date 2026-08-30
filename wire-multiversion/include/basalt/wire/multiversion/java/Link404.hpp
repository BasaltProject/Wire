#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::wire::multiversion::java {

/// 1.14 to 1.13.2, where the light is part of the chunk. 1.14 gave light a packet of its own, so
/// below that the chunk is held back until the light arrives and the two are sent as the one packet
/// 1.13 reads.
///
/// The rest is fields either version has and the other does not.
class Link404 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    struct PendingChunk {
        std::int32_t x = 0;
        std::int32_t z = 0;
        std::uint8_t fullChunk = 0;
        std::int32_t mask = 0;
        std::vector<std::uint8_t> sections;
        std::vector<std::uint8_t> rest;
    };

    void holdChunk(MutablePacket& packet);

    /// The chunk that was held back, with the light of this packet inside its sections.
    ///
    /// The light arrives as one array per section of the column plus one above and one below, which
    /// is why the section at index zero is the one under the world and is skipped.
    [[nodiscard]] bool lightTheChunk(MutablePacket& packet);

    void rewriteLogin(MutablePacket& packet);
    void rewriteRespawn(MutablePacket& packet);
    /// 1.14 made the kind of a spawned thing a varint. Below it there is one byte for it.
    static void narrowType(MutablePacket& packet);

    std::optional<PendingChunk> pending_;
    bool hasSkyLight_ = true;
};

} // namespace basalt::wire::multiversion::java
