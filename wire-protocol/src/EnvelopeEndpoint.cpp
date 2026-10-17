#include <basalt/wire/protocol/EnvelopeEndpoint.hpp>

namespace basalt::wire::protocol::sessionEnvelope {

std::vector<std::uint8_t> EnvelopeEndpoint::open(Edition edition, std::int32_t protocol) {
    if (state_ != State::kIdle)
        return {};
    pending_ = Open{kVersion1, edition, protocol};
    state_ = State::kOpening;
    return encodeControl(*pending_);
}

std::vector<std::uint8_t> EnvelopeEndpoint::data(std::span<const std::uint8_t> packet) const {
    if (state_ != State::kActive || packet.size() > kMaxDataBytes)
        return {};
    return encodeData(packet);
}

std::vector<std::uint8_t> EnvelopeEndpoint::close(CloseReason reason) {
    if (state_ == State::kClosed)
        return {};
    state_ = State::kClosed;
    return encodeControl(Close{reason});
}

EnvelopeEndpoint::Outcome EnvelopeEndpoint::fail(CloseReason reason) {
    Outcome outcome;
    outcome.reply = close(reason);
    outcome.closed = true;
    outcome.reason = reason;
    return outcome;
}

EnvelopeEndpoint::Outcome EnvelopeEndpoint::receive(std::span<const std::uint8_t> frame) {
    if (state_ == State::kClosed)
        return {.closed = true};
    const auto decoded = decode(frame);
    if (!decoded)
        return fail(CloseReason::Malformed);
    if (const auto* data = std::get_if<Data>(&*decoded)) {
        if (state_ != State::kActive)
            return fail(CloseReason::Malformed);
        return {.packet = data->packet};
    }
    const Control& control = std::get<Control>(*decoded);
    if (const auto* closing = std::get_if<Close>(&control)) {
        state_ = State::kClosed;
        return {.closed = true, .reason = closing->reason};
    }
    if (const auto* opened = std::get_if<Opened>(&control)) {
        if (state_ != State::kOpening)
            return fail(CloseReason::Malformed);
        if (opened->version != kVersion1)
            return fail(CloseReason::Unsupported);
        agreed_ = pending_;
        state_ = State::kActive;
        return {};
    }
    const auto& request = std::get<Open>(control);
    if (state_ != State::kIdle)
        return fail(CloseReason::Malformed);
    if (request.version != kVersion1)
        return fail(CloseReason::Unsupported);
    agreed_ = request;
    state_ = State::kActive;
    Outcome outcome;
    outcome.reply = encodeControl(Opened{kVersion1});
    return outcome;
}

} // namespace basalt::wire::protocol::sessionEnvelope
