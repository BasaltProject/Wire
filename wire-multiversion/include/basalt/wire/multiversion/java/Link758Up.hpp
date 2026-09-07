#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <string>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion::java {

/// A 1.19 client on a 1.18.2 server: chat becomes system chat, block actions get their sequence acknowledged.
class Link758Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverboundForeign(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] std::string dimensionKey(const nbt::NbtTag& dimension) const;

    std::vector<std::pair<std::string, nbt::NbtTag>> dimensions_;
    std::vector<std::int32_t> pendingAcks_;
};

} // namespace basalt::wire::multiversion::java
