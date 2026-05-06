#pragma once

#include <basalt/wire/protocol/SessionEnvelope.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace basalt::wire::protocol::sessionEnvelope {

/// One end of the Open/Opened exchange: it says which frame is legal next and what to answer, and never reads past it.
class EnvelopeEndpoint {
public:
    enum class State : std::uint8_t { kIdle, kOpening, kIdentifying, kActive, kClosed };

    struct Outcome {
        /// A packet for the translator, valid until the next call.
        std::span<const std::uint8_t> packet;
        /// A frame to send back at once: the answer to an Open, or the Close that ends a broken exchange.
        std::vector<std::uint8_t> reply;
        bool closed = false;
        std::optional<CloseReason> reason;
        std::string message;
    };

    [[nodiscard]] State state() const noexcept { return state_; }

    [[nodiscard]] const std::optional<Open>& agreed() const noexcept { return agreed_; }

    [[nodiscard]] const std::optional<BedrockIdentity>& identity() const noexcept { return identity_; }

    /// The identity frame that must follow a Bedrock `Open`; empty in any other case.
    [[nodiscard]] std::vector<std::uint8_t> identify(const BedrockIdentity& identity);

    /// The Open frame to send as the last frame of the old codec; empty unless idle.
    [[nodiscard]] std::vector<std::uint8_t> open(Edition edition, std::int32_t protocol);

    /// A data frame for the other end; empty until the exchange is active.
    [[nodiscard]] std::vector<std::uint8_t> data(std::span<const std::uint8_t> packet) const;

    /// The frame that ends an active session with words for the player; empty in any other state.
    [[nodiscard]] std::vector<std::uint8_t> disconnect(std::string message);

    [[nodiscard]] std::vector<std::uint8_t> close(CloseReason reason);

    [[nodiscard]] Outcome receive(std::span<const std::uint8_t> frame);

private:
    [[nodiscard]] Outcome fail(CloseReason reason);

    State state_ = State::kIdle;
    std::optional<Open> pending_;
    std::optional<Open> agreed_;
    std::optional<BedrockIdentity> identity_;
    bool identified_ = false;
};

} // namespace basalt::wire::protocol::sessionEnvelope
