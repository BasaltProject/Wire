#pragma once

#include <cstdint>
#include <span>

namespace basalt::wire::bedrock {

/// Carries finished Bedrock batches to one client; the connection above it does not know which wire it rides on.
class BedrockTransport {
public:
    virtual ~BedrockTransport() = default;

    virtual void sendGame(std::span<const std::uint8_t> payload) = 0;

    /// True when the wire already encrypts and authenticates the bytes, so the Bedrock handshake is skipped.
    [[nodiscard]] virtual bool protectsBytes() const noexcept { return false; }
};

} // namespace basalt::wire::bedrock
