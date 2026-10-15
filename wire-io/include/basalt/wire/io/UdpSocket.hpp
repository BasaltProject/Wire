#pragma once

#include <basalt/wire/io/TcpSocket.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>

namespace basalt::wire::io {

struct UdpEndpoint {
    std::uint32_t address = 0;
    std::uint16_t port = 0;

    [[nodiscard]] friend bool operator==(const UdpEndpoint&, const UdpEndpoint&) noexcept = default;

    [[nodiscard]] std::string toString() const;
};

class UdpSocket {
public:
    UdpSocket() = default;
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;
    ~UdpSocket();

    [[nodiscard]] static std::expected<UdpSocket, SocketError> bind(std::uint16_t port);

    [[nodiscard]] std::expected<std::optional<std::size_t>, SocketError> receiveFrom(std::span<std::uint8_t> into,
                                                                                     UdpEndpoint& from, int timeoutMs);

    /// An unreachable peer's ICMP error surfaces on the next call and is reported as success.
    [[nodiscard]] std::expected<void, SocketError> sendTo(std::span<const std::uint8_t> bytes, const UdpEndpoint& to);

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
