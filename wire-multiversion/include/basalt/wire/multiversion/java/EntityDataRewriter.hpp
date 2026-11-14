#pragma once

#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace basalt::wire::multiversion::java {

/// Both have moved between versions, and a wrong kind is not a field the client ignores but a packet it misreads.
class EntityDataRewriter {
public:
    using StackMapper = std::function<std::optional<protocol::play::WireItemStack>(
        const std::optional<protocol::play::WireItemStack>&)>;

    using StateMapper = std::function<std::int32_t(std::int32_t)>;

    /// `links` is the older side of every link, newest first: an index is moved once per step, in order.
    EntityDataRewriter(std::int32_t farEnd, std::vector<std::int32_t> links, StackMapper stack,
                       StateMapper blockState = {});

    /// A packet from this server, with every value it cannot decode kept as its own bytes.
    [[nodiscard]] static std::optional<protocol::play::SetEntityDataPacket> read(io::ByteBuffer& input);

    /// Fields past `lastKept` are left out, though what an older version derives from them is still written.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> rewrite(const protocol::play::SetEntityDataPacket& data,
                                                                   std::int32_t nativeType,
                                                                   std::uint8_t lastKept = 0xFF) const;

    /// An entity nothing announced the type of: only its shared fields can be placed.
    static constexpr std::int32_t kUnknownType = -1;

private:
    struct Placement {
        std::int32_t index;
        std::int32_t as;
    };

    [[nodiscard]] Placement placementFor(std::int32_t nativeType, std::int32_t nativeIndex,
                                         std::size_t firstStep) const;

    [[nodiscard]] bool replaced(std::int32_t nativeType, std::int32_t nativeIndex) const;

    void writeExtras(io::ByteBuffer& output, const protocol::play::SetEntityDataPacket& data,
                     std::int32_t nativeType) const;

    void writeEntry(io::ByteBuffer& output, const protocol::play::SetEntityDataPacket::Entry& source,
                    Placement placement, std::span<const std::int32_t> types) const;

    void writeAs(io::ByteBuffer& output, const protocol::play::SetEntityDataPacket::Value& value,
                 Placement placement) const;

    [[nodiscard]] std::int32_t entityState(std::int32_t native) const;

    void writeHeader(io::ByteBuffer& output, std::int32_t index, std::int32_t type,
                     std::span<const std::uint8_t> payload) const;

    std::int32_t farEnd_;
    std::vector<std::int32_t> links_;
    StackMapper stack_;
    StateMapper blockState_;
    std::span<const std::int32_t> types_;
    bool packed_;
};

} // namespace basalt::wire::multiversion::java
