#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::wire::protocol {

/// parity: net.minecraft.network.ConnectionProtocol, the five connection states of the protocol.
enum class ProtocolState : std::uint8_t {
    Handshake,
    Status,
    Login,
    Configuration,
    Play,
};

[[nodiscard]] constexpr std::string_view protocolStateName(ProtocolState state) noexcept {
    switch (state) {
    case ProtocolState::Handshake:
        return "handshake";
    case ProtocolState::Status:
        return "status";
    case ProtocolState::Login:
        return "login";
    case ProtocolState::Configuration:
        return "configuration";
    case ProtocolState::Play:
        return "play";
    }
    return "handshake";
}

} // namespace basalt::wire::protocol
