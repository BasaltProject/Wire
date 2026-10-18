#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>
#include <basalt/wire/protocol/ProtocolState.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion {

class MutablePacket {
public:
    std::int32_t id = 0;
    std::vector<std::uint8_t> body;

    [[nodiscard]] std::int32_t nativeId() const noexcept { return nativeId_; }

    void becomes(std::int32_t newNativeId) noexcept { nativeId_ = newNativeId; }

    void overrideId(std::int32_t newId) noexcept {
        id = newId;
        idOverridden_ = true;
    }

    [[nodiscard]] bool idOverridden() const noexcept { return idOverridden_; }

    std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> extras;

    void reset(std::int32_t newId, std::int32_t newNativeId, std::vector<std::uint8_t> newBody);

    [[nodiscard]] bool takeOverride() noexcept;

private:
    std::int32_t nativeId_ = 0;
    bool idOverridden_ = false;
};

/// The entries and attribute keys one version added, which the version below it cannot read.
struct RegistryRemovals {
    NameSet entries{""};
    NameSet attributes{""};

    [[nodiscard]] bool empty() const { return entries.empty() && attributes.empty(); }
};

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

/// The chain renumbers out of the tables; a link only writes the bodies that changed shape.
class ProtocolLink {
public:
    ProtocolLink() = default;
    ProtocolLink(const ProtocolLink&) = default;
    ProtocolLink(ProtocolLink&&) = default;
    ProtocolLink& operator=(const ProtocolLink&) = default;
    ProtocolLink& operator=(ProtocolLink&&) = default;
    virtual ~ProtocolLink() = default;

    [[nodiscard]] virtual std::int32_t older() const = 0;

    [[nodiscard]] virtual const PacketIdTables& packets() const = 0;

    [[nodiscard]] virtual const RegistryRemovals& registryRemovals() const;

    [[nodiscard]] virtual bool clientbound(protocol::ProtocolState state, MutablePacket& packet) = 0;

    [[nodiscard]] virtual bool serverbound(protocol::ProtocolState state, MutablePacket& packet) = 0;

    /// Only in a chain that climbs: a packet the client has that the server has no id for, before ids are renumbered.
    /// True when the link rewrote it as one of the server's, id included.
    [[nodiscard]] virtual bool serverboundForeign(protocol::ProtocolState, MutablePacket&) { return false; }
};

/// The links between what the server speaks and what one client speaks, in order.
class ProtocolChain {
public:
    using Emit = std::function<void(std::int32_t, std::span<const std::uint8_t>)>;

    /// `content` works on the server's side of the links; `tail` on the client's, outside them.
    explicit ProtocolChain(std::vector<std::reference_wrapper<ProtocolLink>> links,
                           PacketContentRewriter* content = nullptr, PacketContentRewriter* tail = nullptr,
                           bool stepwise = false)
        : links_(std::move(links)), content_(content), tail_(tail), stepwise_(stepwise) {}

    [[nodiscard]] bool empty() const noexcept { return links_.empty(); }

    /// Runs a packet on its way to the client and hands the result to the emitter.
    void clientbound(protocol::ProtocolState state, std::int32_t id, std::span<const std::uint8_t> body,
                     const Emit& emit) const;

    /// The same on the way in, oldest link first.
    void serverbound(protocol::ProtocolState state, std::int32_t id, std::span<const std::uint8_t> body,
                     const Emit& emit) const;

    /// Ids at or above this stand for a packet the native version has no counterpart for; the body is never touched.
    static constexpr std::int32_t kOpaqueBase = 0x40000000;

    /// Id-only renumbering for a peer on this chain's version: `ToNative` reads its ids, `FromNative` writes them.
    [[nodiscard]] std::int32_t clientboundIdToNative(protocol::ProtocolState state, std::int32_t id) const;
    [[nodiscard]] std::int32_t clientboundIdFromNative(protocol::ProtocolState state, std::int32_t id) const;
    [[nodiscard]] std::int32_t serverboundIdToNative(protocol::ProtocolState state, std::int32_t id) const;
    [[nodiscard]] std::int32_t serverboundIdFromNative(protocol::ProtocolState state, std::int32_t id) const;

private:
    std::vector<std::reference_wrapper<ProtocolLink>> links_;
    void serverboundStepwise(protocol::ProtocolState state, std::size_t remaining, std::int32_t id,
                             std::span<const std::uint8_t> body, const Emit& emit) const;

    PacketContentRewriter* content_;
    PacketContentRewriter* tail_;
    bool stepwise_;
};

} // namespace basalt::wire::multiversion
