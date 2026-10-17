#pragma once

#include <basalt/wire/protocol/SessionEnvelope.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace basalt::wire::protocol::sessionEnvelope {

/// One end of the Open/Opened exchange: it says which frame is legal next and what to answer, and never reads past it.
class EnvelopeEndpoint {
public:
    enum class State : std::uint8_t { kIdle, kOpening, kActive, kClosed };

    struct Outcome {
        /// A packet for the translator, valid until the next call.
        std::span<const std::uint8_t> packet;
        /// A frame to send back at once: the answer to an Open, or the Close that ends a broken exchange.
        std::vector<std::uint8_t> reply;
        bool closed = false;
        std::optional<CloseReason> reason;
    };

    [[nodiscard]] State state() const noexcept { return state_; }

    [[nodiscard]] const std::optional<Open>& agreed() const noexcept { return agreed_; }

    /// The Open frame to send as the last frame of the old codec; empty unless idle.
    [[nodiscard]] std::vector<std::uint8_t> open(Edition edition, std::int32_t protocol);

    /// A data frame for the other end; empty until the exchange is active.
    [[nodiscard]] std::vector<std::uint8_t> data(std::span<const std::uint8_t> packet) const;

    [[nodiscard]] std::vector<std::uint8_t> close(CloseReason reason);

    [[nodiscard]] Outcome receive(std::span<const std::uint8_t> frame);

private:
    [[nodiscard]] Outcome fail(CloseReason reason);

    State state_ = State::kIdle;
    std::optional<Open> pending_;
    std::optional<Open> agreed_;
};

} // namespace basalt::wire::protocol::sessionEnvelope
