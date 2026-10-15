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

    /// `links` is the older side of every link, newest first: an index is moved once per step, in order.
    EntityDataRewriter(std::int32_t farEnd, std::vector<std::int32_t> links, StackMapper stack);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> rewrite(const protocol::play::SetEntityDataPacket& data,
                                                                   std::int32_t nativeType) const;

    /// An entity nothing announced the type of: only its shared fields can be placed.
    static constexpr std::int32_t kUnknownType = -1;

private:
    [[nodiscard]] std::int32_t indexFor(std::int32_t nativeType, std::int32_t nativeIndex) const;

    void writeEntry(io::ByteBuffer& output, const protocol::play::SetEntityDataPacket::Entry& source,
                    std::int32_t index, std::span<const std::int32_t> types) const;

    std::int32_t farEnd_;
    std::vector<std::int32_t> links_;
    StackMapper stack_;
    std::span<const std::int32_t> types_;
    bool packed_;
};

} // namespace basalt::wire::multiversion::java
