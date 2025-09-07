#include <basalt/wire/io/TcpSocket.hpp>

#include <charconv>
#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_type = int;
// Winsock's recv and send take an int length; POSIX takes a size_t.
using iolen_type = int;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socklen_type = socklen_t;
using iolen_type = std::size_t;
#endif

namespace basalt::wire::io {
namespace {

/// One-time socket subsystem setup: nothing on POSIX, WSAStartup on Windows, which Winsock demands
/// before any other socket call.
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

/// The peer is gone, which read and write treat as end of stream rather than as failure.
bool errorIsPeerGone(int error) noexcept {
#if defined(_WIN32)
    return error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENOTCONN || error == WSAESHUTDOWN;
#else
    return error == ECONNRESET || error == EPIPE || error == ENOTCONN || error == ECONNABORTED;
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

std::int64_t createSocket() {
#if defined(_WIN32)
    const SOCKET raw = ::socket(AF_INET, SOCK_STREAM, 0);
    return raw == INVALID_SOCKET ? -1 : static_cast<std::int64_t>(raw);
#else
    return ::socket(AF_INET, SOCK_STREAM, 0);
#endif
}

#if defined(_WIN32)
SOCKET asSocket(std::int64_t handle) noexcept { return static_cast<SOCKET>(handle); }
#else
int asSocket(std::int64_t handle) noexcept { return static_cast<int>(handle); }
#endif

} // namespace

std::optional<std::uint32_t> parseIpv4(std::string_view address) noexcept {
    std::uint32_t result = 0;
    std::size_t cursor = 0;

    for (int octet = 0; octet < 4; ++octet) {
        const bool last = octet == 3;
        const std::size_t dot = address.find('.', cursor);
        if (last != (dot == std::string_view::npos)) {
            return std::nullopt;
        }
        const std::size_t end = last ? address.size() : dot;

        std::uint32_t value = 0;
        const auto parsed = std::from_chars(address.data() + cursor, address.data() + end, value);
        if (parsed.ec != std::errc{} || parsed.ptr != address.data() + end || value > 255) {
            return std::nullopt;
        }
        result |= value << (8 * octet);
        cursor = end + 1;
    }
    return result;
}

std::string formatIpv4(std::uint32_t networkOrder) {
    std::string out;
    for (int i = 0; i < 4; ++i) {
        if (i > 0) {
            out += '.';
        }
        out += std::to_string((networkOrder >> (8 * i)) & 0xFF);
    }
    return out;
}

TcpConnection::TcpConnection(TcpConnection&& other) noexcept
    : handle_(other.handle_), remoteAddress_(std::move(other.remoteAddress_)) {
    other.handle_ = -1;
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        remoteAddress_ = std::move(other.remoteAddress_);
        other.handle_ = -1;
    }
    return *this;
}

TcpConnection::~TcpConnection() { close(); }

void TcpConnection::close() noexcept {
    closeHandle(handle_);
    handle_ = -1;
}

std::expected<TcpConnection, SocketError> TcpConnection::connect(std::string_view host, std::uint16_t port) {
    ensureSocketsInitialized();

    const std::optional<std::uint32_t> address = parseIpv4(host);
    if (!address) {
        return std::unexpected{SocketError{SocketError::Kind::InvalidAddress, 0}};
    }

    const std::int64_t handle = createSocket();
    if (handle < 0) {
        return std::unexpected{SocketError{SocketError::Kind::CreateFailed, lastSocketError()}};
    }

    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    std::memcpy(&target.sin_addr, &*address, sizeof(std::uint32_t));

    if (::connect(asSocket(handle), reinterpret_cast<const sockaddr*>(&target), sizeof(target)) != 0) {
        const int error = lastSocketError();
        closeHandle(handle);
        return std::unexpected{SocketError{SocketError::Kind::ConnectFailed, error}};
    }
    return TcpConnection{handle};
}

std::expected<std::size_t, SocketError> TcpConnection::read(std::span<std::uint8_t> into) {
    if (isClosed() || into.empty()) {
        return std::size_t{0};
    }
    while (true) {
        const auto received =
            ::recv(asSocket(handle_), reinterpret_cast<char*>(into.data()), static_cast<iolen_type>(into.size()), 0);
        if (received >= 0) {
            return static_cast<std::size_t>(received);
        }
        const int error = lastSocketError();
        if (errorIsInterrupt(error)) {
            continue;
        }
        if (errorIsPeerGone(error)) {
            return std::size_t{0};
        }
        return std::unexpected{SocketError{SocketError::Kind::ReceiveFailed, error}};
    }
}

std::expected<void, SocketError> TcpConnection::write(std::span<const std::uint8_t> bytes) {
    if (isClosed()) {
        return {};
    }
    std::size_t written = 0;
    while (written < bytes.size()) {
        const auto sent = ::send(asSocket(handle_), reinterpret_cast<const char*>(bytes.data() + written),
                                 static_cast<iolen_type>(bytes.size() - written), 0);
        if (sent < 0) {
            const int error = lastSocketError();
            if (errorIsInterrupt(error)) {
                continue;
            }
            if (errorIsPeerGone(error)) {
                return {};
            }
            return std::unexpected{SocketError{SocketError::Kind::SendFailed, error}};
        }
        written += static_cast<std::size_t>(sent);
    }
    return {};
}

TcpListener::TcpListener(TcpListener&& other) noexcept : handle_(other.handle_) { other.handle_ = -1; }

TcpListener& TcpListener::operator=(TcpListener&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        other.handle_ = -1;
    }
    return *this;
}

TcpListener::~TcpListener() { close(); }

void TcpListener::close() noexcept {
    closeHandle(handle_);
    handle_ = -1;
}

std::expected<TcpListener, SocketError> TcpListener::bind(std::uint16_t port, int backlog) {
    ensureSocketsInitialized();

    const std::int64_t handle = createSocket();
    if (handle < 0) {
        return std::unexpected{SocketError{SocketError::Kind::CreateFailed, lastSocketError()}};
    }

    // Restarting the server should not have to wait out the previous socket's TIME_WAIT.
    const int reuse = 1;
    ::setsockopt(asSocket(handle), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    if (::bind(asSocket(handle), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        const int error = lastSocketError();
        closeHandle(handle);
        return std::unexpected{SocketError{SocketError::Kind::BindFailed, error}};
    }
    if (::listen(asSocket(handle), backlog) != 0) {
        const int error = lastSocketError();
        closeHandle(handle);
        return std::unexpected{SocketError{SocketError::Kind::ListenFailed, error}};
    }
    return TcpListener{handle};
}

std::expected<TcpConnection, SocketError> TcpListener::accept() {
    sockaddr_in peer{};
    auto length = static_cast<socklen_type>(sizeof(peer));
    while (true) {
        const auto raw = ::accept(asSocket(handle_), reinterpret_cast<sockaddr*>(&peer), &length);
#if defined(_WIN32)
        const std::int64_t clientHandle = raw == INVALID_SOCKET ? -1 : static_cast<std::int64_t>(raw);
#else
        const std::int64_t clientHandle = raw;
#endif
        if (clientHandle >= 0) {
            std::uint32_t address = 0;
            std::memcpy(&address, &peer.sin_addr, sizeof(address));
            return TcpConnection{clientHandle, formatIpv4(address)};
        }
        const int error = lastSocketError();
        if (errorIsInterrupt(error)) {
            continue;
        }
        return std::unexpected{SocketError{SocketError::Kind::AcceptFailed, error}};
    }
}

std::expected<std::uint16_t, SocketError> TcpListener::localPort() const {
    sockaddr_in address{};
    auto length = static_cast<socklen_type>(sizeof(address));
    if (::getsockname(asSocket(handle_), reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return std::unexpected{SocketError{SocketError::Kind::BindFailed, lastSocketError()}};
    }
    return ntohs(address.sin_port);
}

} // namespace basalt::wire::io
