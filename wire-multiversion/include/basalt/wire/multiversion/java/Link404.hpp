#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::wire::multiversion::java {

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

    [[nodiscard]] bool lightTheChunk(MutablePacket& packet);

    void rewriteLogin(MutablePacket& packet);
    void rewriteRespawn(MutablePacket& packet);
    static void narrowType(MutablePacket& packet);

    std::optional<PendingChunk> pending_;
    bool hasSkyLight_ = true;
};

} // namespace basalt::wire::multiversion::java
