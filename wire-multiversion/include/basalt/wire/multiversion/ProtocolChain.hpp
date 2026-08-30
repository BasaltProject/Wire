#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>
#include <basalt/wire/protocol/ProtocolState.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion {

/// A packet on its way through the chain: an id and a body, both of which a link may replace.
///
/// The id is what the version above the current link calls it, a number no link has constants for.
/// The native id is what this server calls it, which is what a link asks to know what it is looking
/// at.
class MutablePacket {
public:
    std::int32_t id = 0;
    std::vector<std::uint8_t> body;

    /// What this server calls this packet, whatever the versions in between call it.
    [[nodiscard]] std::int32_t nativeId() const noexcept { return nativeId_; }

    /// Says the packet is now the one that id names, for a link that made it into that: a chunk
    /// folded into the light packet is a chunk from there on.
    void becomes(std::int32_t newNativeId) noexcept { nativeId_ = newNativeId; }

    /// Says what this link's older side calls the packet, when the link knows better than its table.
    ///
    /// Needed where a packet was renamed or replaced: a table matches by name, so it sees the old
    /// name as gone and would drop it. What is named here is the id *at that link*, and the links
    /// below carry on renumbering it from there.
    void overrideId(std::int32_t newId) noexcept {
        id = newId;
        idOverridden_ = true;
    }

    [[nodiscard]] bool idOverridden() const noexcept { return idOverridden_; }

    /// Packets this one turned into on top of itself, sent after it. Empty in almost every link.
    std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> extras;

    void reset(std::int32_t newId, std::int32_t newNativeId, std::vector<std::uint8_t> newBody);

    /// Consumes the override, which is how the chain tells a named packet from a renumbered one.
    [[nodiscard]] bool takeOverride() noexcept;

private:
    std::int32_t nativeId_ = 0;
    bool idOverridden_ = false;
};

/// The entries and attribute keys one version added, which the version below it cannot read.
///
/// An element naming something that does not exist over there is not a smaller world: the client
/// refuses to load its registries at all and never reaches the game, which is what a real client
/// does when it is sent a 26.2 biome and a 26.2 song.
struct RegistryRemovals {
    NameSet entries{""};
    NameSet attributes{""};

    [[nodiscard]] bool empty() const { return entries.empty() && attributes.empty(); }
};

/// Whatever renumbers what a packet carries, for a whole chain at once.
///
/// Its own interface rather than a link so that the chain can run it exactly once, on the packet as
/// this server wrote it: a link halfway down is looking at a packet the links above it have already
/// rewritten, and cannot read its fields.
class PacketContentRewriter {
public:
    PacketContentRewriter() = default;
    PacketContentRewriter(const PacketContentRewriter&) = default;
    PacketContentRewriter(PacketContentRewriter&&) = default;
    PacketContentRewriter& operator=(const PacketContentRewriter&) = default;
    PacketContentRewriter& operator=(PacketContentRewriter&&) = default;
    virtual ~PacketContentRewriter() = default;

    [[nodiscard]] virtual bool clientbound(protocol::ProtocolState state, MutablePacket& packet) = 0;

    [[nodiscard]] virtual bool serverbound(protocol::ProtocolState state, MutablePacket& packet) = 0;
};

/// One step between two neighbouring protocols. A link knows only about its two versions and the
/// chain composes them, so 1.21 is reached by running the same small steps rather than by anybody
/// writing "1.21 to 26.2".
///
/// The chain renumbers out of the tables; a link only writes the bodies that changed shape.
/// Returning false drops the packet, which is how a link handles something the older version lacks.
class ProtocolLink {
public:
    ProtocolLink() = default;
    ProtocolLink(const ProtocolLink&) = default;
    ProtocolLink(ProtocolLink&&) = default;
    ProtocolLink& operator=(const ProtocolLink&) = default;
    ProtocolLink& operator=(ProtocolLink&&) = default;
    virtual ~ProtocolLink() = default;

    /// The older side. The newer side is the older of the next link up, or the native version.
    [[nodiscard]] virtual std::int32_t older() const = 0;

    /// What the older side calls each of the newer side's packets.
    [[nodiscard]] virtual const PacketIdTables& packets() const = 0;

    /// What the older side cannot be told, in the registries this server sends it.
    ///
    /// Applied by the chain rather than by the link, because it is the same work every step and
    /// because every step has to do it: a client four versions back has to be spared what each of
    /// the four added.
    [[nodiscard]] virtual const RegistryRemovals& registryRemovals() const;

    [[nodiscard]] virtual bool clientbound(protocol::ProtocolState state, MutablePacket& packet) = 0;

    [[nodiscard]] virtual bool serverbound(protocol::ProtocolState state, MutablePacket& packet) = 0;
};

/// The links between what the server speaks and what one client speaks, in order.
///
/// Clientbound runs newest to oldest and serverbound runs the other way, which is the whole idea:
/// every link is written once, against the version right below it, and a client five versions back
/// is five hops rather than a translator of its own.
class ProtocolChain {
public:
    using Emit = std::function<void(std::int32_t, std::span<const std::uint8_t>)>;

    explicit ProtocolChain(std::vector<std::reference_wrapper<ProtocolLink>> links,
                           PacketContentRewriter* content = nullptr)
        : links_(std::move(links)), content_(content) {}

    [[nodiscard]] bool empty() const noexcept { return links_.empty(); }

    /// Runs a packet on its way to the client and hands the result to the emitter.
    void clientbound(protocol::ProtocolState state, std::int32_t id, std::span<const std::uint8_t> body,
                     const Emit& emit) const;

    /// The same on the way in, oldest link first.
    ///
    /// Renumbering runs to the end before any body is touched, so that every link is looking at the
    /// packet this server would have read, not at the number the version below it happened to use.
    void serverbound(protocol::ProtocolState state, std::int32_t id, std::span<const std::uint8_t> body,
                     const Emit& emit) const;

private:
    std::vector<std::reference_wrapper<ProtocolLink>> links_;
    PacketContentRewriter* content_;
};

} // namespace basalt::wire::multiversion
