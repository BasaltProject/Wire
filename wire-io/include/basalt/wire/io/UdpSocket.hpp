#pragma once

#include <basalt/wire/io/TcpSocket.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>

namespace basalt::wire::io {

/// Where a datagram came from, or is going to. IPv4 only, like the rest of this layer.
struct UdpEndpoint {
    /// Network order, the same shape parseIpv4 returns.
    std::uint32_t address = 0;
    std::uint16_t port = 0;

    [[nodiscard]] friend bool operator==(const UdpEndpoint&, const UdpEndpoint&) noexcept = default;

    [[nodiscard]] std::string toString() const;
};

/// An unconnected IPv4 UDP socket. One socket serves every peer, which is what RakNet expects: a
/// session is an address and a port, not a file descriptor.
class UdpSocket {
public:
    UdpSocket() = default;
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;
    ~UdpSocket();

    [[nodiscard]] static std::expected<UdpSocket, SocketError> bind(std::uint16_t port);

    /// Waits up to timeoutMs for one datagram. An empty result is the timeout, not a failure, and a
    /// datagram of zero bytes is a real (if useless) datagram, so the two cannot share a value.
    [[nodiscard]] std::expected<std::optional<std::size_t>, SocketError> receiveFrom(std::span<std::uint8_t> into,
                                                                                     UdpEndpoint& from, int timeoutMs);

    /// A datagram goes out whole or not at all, so there is no partial write to loop over. An
    /// unreachable peer answers with an ICMP error that the next call reports; that is ordinary and
    /// comes back as success, because UDP promises nothing.
    [[nodiscard]] std::expected<void, SocketError> sendTo(std::span<const std::uint8_t> bytes, const UdpEndpoint& to);

    /// The port actually bound, which is what port 0 was resolved to.
    [[nodiscard]] std::expected<std::uint16_t, SocketError> localPort() const;

    [[nodiscard]] std::int64_t nativeHandle() const noexcept { return handle_; }

    [[nodiscard]] bool isClosed() const noexcept { return handle_ < 0; }

    void close() noexcept;

private:
    explicit UdpSocket(std::int64_t handle) : handle_(handle) {}

    std::int64_t handle_ = -1;
};

} // namespace basalt::wire::io

template <>
struct std::hash<basalt::wire::io::UdpEndpoint> {
    [[nodiscard]] std::size_t operator()(const basalt::wire::io::UdpEndpoint& endpoint) const noexcept {
        return (static_cast<std::size_t>(endpoint.address) << 16U) ^ endpoint.port;
    }
};
