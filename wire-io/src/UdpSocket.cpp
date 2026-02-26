#include <basalt/wire/io/UdpSocket.hpp>

#include <cstring>
#include <utility>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_type = int;
using iolen_type = int;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
using socklen_type = socklen_t;
using iolen_type = std::size_t;
#endif

namespace basalt::wire::io {
namespace {

void ensureSocketsInitialized() {
#if defined(_WIN32)
    static const bool started = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    static_cast<void>(started);
#endif
}

int lastSocketError() noexcept {
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool errorIsInterrupt(int error) noexcept {
#if defined(_WIN32)
    return error == WSAEINTR;
#else
    return error == EINTR;
#endif
}

/// An ICMP port-unreachable from a peer that went away surfaces on the next call on this socket,
/// even though nothing about this call failed. Reporting it would make a session's death look like
/// a socket fault, so the datagram loop treats it as an empty read.
bool errorIsPeerGone(int error) noexcept {
#if defined(_WIN32)
    return error == WSAECONNRESET || error == WSAENETRESET || error == WSAECONNREFUSED;
#else
    return error == ECONNREFUSED || error == ECONNRESET || error == EHOSTUNREACH || error == ENETUNREACH;
#endif
}

void closeHandle(std::int64_t handle) noexcept {
    if (handle < 0) {
        return;
    }
#if defined(_WIN32)
    ::closesocket(static_cast<SOCKET>(handle));
#else
    ::close(static_cast<int>(handle));
#endif
}

sockaddr_in toSockaddr(const UdpEndpoint& endpoint) noexcept {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    std::memcpy(&address.sin_addr, &endpoint.address, sizeof(std::uint32_t));
    return address;
}

/// Blocks up to timeoutMs waiting for the socket to have something to read.
[[nodiscard]] int waitReadable(std::int64_t handle, int timeoutMs) noexcept {
#if defined(_WIN32)
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(static_cast<SOCKET>(handle), &readable);
    timeval limit{};
    limit.tv_sec = timeoutMs / 1000;
    limit.tv_usec = (timeoutMs % 1000) * 1000;
    return ::select(0, &readable, nullptr, nullptr, &limit);
#else
    pollfd watched{};
    watched.fd = static_cast<int>(handle);
    watched.events = POLLIN;
    return ::poll(&watched, 1, timeoutMs);
#endif
}

} // namespace

std::string UdpEndpoint::toString() const { return formatIpv4(address) + ':' + std::to_string(port); }

UdpSocket::UdpSocket(UdpSocket&& other) noexcept : handle_(std::exchange(other.handle_, -1)) {}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = std::exchange(other.handle_, -1);
    }
    return *this;
}

UdpSocket::~UdpSocket() { close(); }

std::expected<UdpSocket, SocketError> UdpSocket::bind(std::uint16_t port) {
    ensureSocketsInitialized();

#if defined(_WIN32)
    const SOCKET raw = ::socket(AF_INET, SOCK_DGRAM, 0);
    const std::int64_t handle = raw == INVALID_SOCKET ? -1 : static_cast<std::int64_t>(raw);
#else
    const std::int64_t handle = ::socket(AF_INET, SOCK_DGRAM, 0);
#endif
    if (handle < 0) {
        return std::unexpected(SocketError{SocketError::Kind::CreateFailed, lastSocketError()});
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (::bind(static_cast<int>(handle), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        const int error = lastSocketError();
        closeHandle(handle);
        return std::unexpected(SocketError{SocketError::Kind::BindFailed, error});
    }

    return UdpSocket{handle};
}

std::expected<std::optional<std::size_t>, SocketError> UdpSocket::receiveFrom(std::span<std::uint8_t> into,
                                                                              UdpEndpoint& from, int timeoutMs) {
    if (handle_ < 0) {
        return std::unexpected(SocketError{SocketError::Kind::ReceiveFailed, 0});
    }

    const int ready = waitReadable(handle_, timeoutMs);
    if (ready < 0) {
        const int error = lastSocketError();
        if (errorIsInterrupt(error)) {
            return std::optional<std::size_t>{};
        }
        return std::unexpected(SocketError{SocketError::Kind::ReceiveFailed, error});
    }
    if (ready == 0) {
        return std::optional<std::size_t>{};
    }

    sockaddr_in peer{};
    socklen_type peerLength = sizeof(peer);
    const auto received =
        ::recvfrom(static_cast<int>(handle_), reinterpret_cast<char*>(into.data()),
                   static_cast<iolen_type>(into.size()), 0, reinterpret_cast<sockaddr*>(&peer), &peerLength);
    if (received < 0) {
        const int error = lastSocketError();
        if (errorIsInterrupt(error) || errorIsPeerGone(error)) {
            return std::optional<std::size_t>{};
        }
        return std::unexpected(SocketError{SocketError::Kind::ReceiveFailed, error});
    }

    std::memcpy(&from.address, &peer.sin_addr, sizeof(std::uint32_t));
    from.port = ntohs(peer.sin_port);
    return std::optional<std::size_t>{static_cast<std::size_t>(received)};
}

std::expected<void, SocketError> UdpSocket::sendTo(std::span<const std::uint8_t> bytes, const UdpEndpoint& to) {
    if (handle_ < 0) {
        return std::unexpected(SocketError{SocketError::Kind::SendFailed, 0});
    }

    const sockaddr_in address = toSockaddr(to);
    const auto sent = ::sendto(static_cast<int>(handle_), reinterpret_cast<const char*>(bytes.data()),
                               static_cast<iolen_type>(bytes.size()), 0, reinterpret_cast<const sockaddr*>(&address),
                               sizeof(address));
    if (sent < 0) {
        const int error = lastSocketError();
        if (errorIsInterrupt(error) || errorIsPeerGone(error)) {
            return {};
        }
        return std::unexpected(SocketError{SocketError::Kind::SendFailed, error});
    }
    return {};
}

std::expected<std::uint16_t, SocketError> UdpSocket::localPort() const {
    if (handle_ < 0) {
        return std::unexpected(SocketError{SocketError::Kind::BindFailed, 0});
    }

    sockaddr_in address{};
    socklen_type length = sizeof(address);
    if (::getsockname(static_cast<int>(handle_), reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return std::unexpected(SocketError{SocketError::Kind::BindFailed, lastSocketError()});
    }
    return ntohs(address.sin_port);
}

void UdpSocket::close() noexcept {
    closeHandle(handle_);
    handle_ = -1;
}

} // namespace basalt::wire::io
