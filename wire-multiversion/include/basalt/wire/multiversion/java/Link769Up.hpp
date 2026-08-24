#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

/// A 1.21.5 client on a 1.21.4 server: heightmaps, chat indexes, text events, hashed clicks and item data.
class Link769Up final : public RenumberingLink {
public:
    Link769Up(const VersionTables& upward, std::int32_t backendVersion);

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    std::int32_t messageIndex_ = 0;
    bool intModelData_;
};

} // namespace basalt::wire::multiversion::java
