#include <basalt/wire/io/TcpSocket.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_type = int;
using namelen_type = DWORD;
// Winsock's recv and send take an int length; POSIX takes a size_t.
using iolen_type = int;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socklen_type = socklen_t;
using namelen_type = socklen_t;
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

bool errorWouldBlock(int error) noexcept {
#if defined(_WIN32)
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
#else
    return error == EAGAIN || error == EWOULDBLOCK || error == EINPROGRESS;
#endif
}

bool retryableAcceptError(int error) noexcept {
#if defined(_WIN32)
    return error == WSAECONNRESET || error == WSAECONNABORTED;
#else
    if (error == ECONNABORTED || error == ECONNRESET || error == EPROTO)
        return true;
#if defined(__linux__)
    // Linux exposes queued-child network errors through accept; retry them like EAGAIN.
    return error == ENETDOWN || error == ENOPROTOOPT || error == EHOSTDOWN || error == ENONET ||
           error == EHOSTUNREACH || error == EOPNOTSUPP || error == ENETUNREACH;
#else
    return false;
#endif
#endif
}

std::expected<void, SocketError> nonBlocking(std::int64_t handle, bool enabled) {
#if defined(_WIN32)
    u_long mode = enabled ? 1 : 0;
    const int result = ::ioctlsocket(static_cast<SOCKET>(handle), FIONBIO, &mode);
#else
    const int flags = ::fcntl(static_cast<int>(handle), F_GETFL, 0);
    const int result =
        flags < 0 ? -1 : ::fcntl(static_cast<int>(handle), F_SETFL, enabled ? flags | O_NONBLOCK : flags & ~O_NONBLOCK);
#endif
    if (result != 0) {
        return std::unexpected{SocketError{SocketError::Kind::ConfigureFailed, lastSocketError()}};
    }
    return {};
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

std::int64_t createSocket(int family = AF_INET) {
#if defined(_WIN32)
    const SOCKET raw = ::socket(family, SOCK_STREAM, 0);
    return raw == INVALID_SOCKET ? -1 : static_cast<std::int64_t>(raw);
#else
    const int handle = ::socket(family, SOCK_STREAM, 0);
#if defined(__APPLE__)
    const int noSignal = 1;
    if (handle >= 0) {
        ::setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
    }
#endif
    return handle;
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

std::expected<std::vector<TcpAddress>, SocketError> TcpAddress::lookup(std::string_view host, std::uint16_t port,
                                                                       bool numeric) {
    ensureSocketsInitialized();
    if (host.empty() || host.find('\0') != std::string_view::npos) {
        return std::unexpected{SocketError{SocketError::Kind::InvalidAddress, 0}};
    }
    const std::string name{host};
    const std::string service = std::to_string(port);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICSERV | (numeric ? AI_NUMERICHOST : 0);
    addrinfo* raw = nullptr;
    const int error = ::getaddrinfo(name.c_str(), service.c_str(), &hints, &raw);
    if (error != 0) {
        return std::unexpected{
            SocketError{numeric ? SocketError::Kind::InvalidAddress : SocketError::Kind::ResolveFailed, error}};
    }
    const std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> owned{raw, &::freeaddrinfo};
    std::vector<TcpAddress> addresses;
    for (const addrinfo* current = raw; current != nullptr; current = current->ai_next) {
        if ((current->ai_family != AF_INET && current->ai_family != AF_INET6) || current->ai_addrlen > 128) {
            continue;
        }
        TcpAddress address;
        address.family_ = current->ai_family;
        address.size_ = static_cast<std::uint32_t>(current->ai_addrlen);
        address.port_ = port;
        std::memcpy(address.storage_.data(), current->ai_addr, address.size_);
        if (!std::ranges::any_of(addresses, [&](const TcpAddress& other) {
                return other.family_ == address.family_ && other.size_ == address.size_ &&
                       other.storage_ == address.storage_;
            })) {
            addresses.push_back(address);
        }
    }
    if (addresses.empty()) {
        return std::unexpected{SocketError{SocketError::Kind::InvalidAddress, 0}};
    }
    return addresses;
}

std::expected<std::vector<TcpAddress>, SocketError> TcpAddress::resolve(std::string_view host, std::uint16_t port) {
    return lookup(host, port, false);
}

std::expected<TcpAddress, SocketError> TcpAddress::literal(std::string_view host, std::uint16_t port) {
    if (host.find(':') == std::string_view::npos) {
        const auto ipv4 = parseIpv4(host);
        if (!ipv4) {
            return std::unexpected{SocketError{SocketError::Kind::InvalidAddress, 0}};
        }
        sockaddr_in native{};
        native.sin_family = AF_INET;
        native.sin_port = htons(port);
        native.sin_addr.s_addr = *ipv4;
        TcpAddress address;
        address.family_ = AF_INET;
        address.size_ = sizeof(native);
        address.port_ = port;
        std::memcpy(address.storage_.data(), &native, sizeof(native));
        return address;
    }
    auto addresses = lookup(host, port, true);
    if (!addresses) {
        return std::unexpected{addresses.error()};
    }
    return addresses->front();
}

std::string TcpAddress::host() const {
    std::array<char, NI_MAXHOST> text{};
    if (size_ == 0 ||
        ::getnameinfo(reinterpret_cast<const sockaddr*>(storage_.data()), static_cast<socklen_type>(size_), text.data(),
                      static_cast<namelen_type>(text.size()), nullptr, 0, NI_NUMERICHOST) != 0) {
        return {};
    }
    return std::string{text.data()};
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
    // Keep the blocking convenience method's literal-only contract. Resolve explicitly on a worker.
    const auto address = TcpAddress::literal(host, port);
    if (!address) {
        return std::unexpected{address.error()};
    }
    return connect(*address);
}

std::expected<TcpConnection, SocketError> TcpConnection::connect(const TcpAddress& address) {
    return connectAddress(address, false);
}

std::expected<TcpConnection, SocketError> TcpConnection::connectAddress(const TcpAddress& address, bool nonblocking) {
    ensureSocketsInitialized();
    if (address.size_ == 0) {
        return std::unexpected{SocketError{SocketError::Kind::InvalidAddress, 0}};
    }
    TcpConnection connection{createSocket(address.family_)};
    if (connection.isClosed()) {
        return std::unexpected{SocketError{SocketError::Kind::CreateFailed, lastSocketError()}};
    }
    if (nonblocking) {
        const auto configured = connection.setNonBlocking();
        if (!configured) {
            return std::unexpected{configured.error()};
        }
    }
    if (::connect(asSocket(connection.handle_), reinterpret_cast<const sockaddr*>(address.storage_.data()),
                  static_cast<socklen_type>(address.size_)) != 0) {
        const int error = lastSocketError();
        if (!nonblocking || !errorWouldBlock(error)) {
            return std::unexpected{SocketError{SocketError::Kind::ConnectFailed, error}};
        }
    }
    return connection;
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
        if (errorWouldBlock(error)) {
            return std::unexpected{SocketError{SocketError::Kind::WouldBlock, error}};
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
        const auto sent = writeSome(bytes.subspan(written));
        if (!sent) {
            if (errorIsPeerGone(sent.error().platformCode)) {
                return {};
            }
            return std::unexpected{sent.error()};
        }
        written += *sent;
    }
    return {};
}

std::expected<std::size_t, SocketError> TcpConnection::writeSome(std::span<const std::uint8_t> bytes) {
    if (bytes.empty()) {
        return std::size_t{0};
    }
    const auto length =
        static_cast<iolen_type>(std::min(bytes.size(), static_cast<std::size_t>(std::numeric_limits<int>::max())));
#if defined(MSG_NOSIGNAL)
    constexpr int kSendFlags = MSG_NOSIGNAL;
#else
    constexpr int kSendFlags = 0;
#endif
    while (true) {
        const auto sent = ::send(asSocket(handle_), reinterpret_cast<const char*>(bytes.data()), length, kSendFlags);
        if (sent > 0) {
            return static_cast<std::size_t>(sent);
        }
        const int error = sent == 0 ? 0 : lastSocketError();
        if (errorIsInterrupt(error)) {
            continue;
        }
        return std::unexpected{
            SocketError{errorWouldBlock(error) ? SocketError::Kind::WouldBlock : SocketError::Kind::SendFailed, error}};
    }
}

std::expected<void, SocketError> TcpConnection::setNonBlocking(bool enabled) { return nonBlocking(handle_, enabled); }

std::expected<bool, SocketError> TcpConnection::finishConnect() {
    int error = 0;
    auto size = static_cast<socklen_type>(sizeof(error));
    if (::getsockopt(asSocket(handle_), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) != 0) {
        return std::unexpected{SocketError{SocketError::Kind::ConnectFailed, lastSocketError()}};
    }
    if (errorWouldBlock(error)) {
        return false;
    }
    if (error != 0) {
        return std::unexpected{SocketError{SocketError::Kind::ConnectFailed, error}};
    }
    // SO_ERROR can be zero while the handshake is still in flight.
    sockaddr_storage peer{};
    size = static_cast<socklen_type>(sizeof(peer));
    if (::getpeername(asSocket(handle_), reinterpret_cast<sockaddr*>(&peer), &size) != 0) {
        const int code = lastSocketError();
#if defined(_WIN32)
        if (code == WSAENOTCONN) {
#else
        if (code == ENOTCONN) {
#endif
            return false;
        }
        return std::unexpected{SocketError{SocketError::Kind::ConnectFailed, code}};
    }
    return true;
}

std::expected<TcpConnection, SocketError> TcpConnection::connectNonBlocking(std::string_view host, std::uint16_t port) {
    const auto address = TcpAddress::literal(host, port);
    if (!address) {
        return std::unexpected{address.error()};
    }
    return connectNonBlocking(*address);
}

std::expected<TcpConnection, SocketError> TcpConnection::connectNonBlocking(const TcpAddress& address) {
    return connectAddress(address, true);
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

std::expected<void, SocketError> TcpListener::setNonBlocking(bool enabled) { return nonBlocking(handle_, enabled); }

void TcpListener::close() noexcept {
    // Closing the descriptor wakes a blocked accept() on macOS and Windows, but not on Linux: the
    // acceptor thread stays parked until a connection arrives, so the process never exits. Shutting
    // the socket down first is what breaks it there, and is a harmless no-op elsewhere.
    if (handle_ >= 0) {
#if defined(_WIN32)
        ::shutdown(static_cast<SOCKET>(handle_), SD_BOTH);
#else
        ::shutdown(static_cast<int>(handle_), SHUT_RDWR);
#endif
    }
    closeHandle(handle_);
    handle_ = -1;
}

std::expected<TcpListener, SocketError> TcpListener::bind(std::uint16_t port, int backlog) {
    const auto address = TcpAddress::literal("0.0.0.0", port);
    if (!address) {
        return std::unexpected{address.error()};
    }
    return bind(*address, backlog);
}

std::expected<TcpListener, SocketError> TcpListener::bind(const TcpAddress& address, int backlog, bool dualStack) {
    ensureSocketsInitialized();
    if (address.size_ == 0) {
        return std::unexpected{SocketError{SocketError::Kind::InvalidAddress, 0}};
    }
    TcpListener listener{createSocket(address.family_)};
    if (listener.handle_ < 0) {
        return std::unexpected{SocketError{SocketError::Kind::CreateFailed, lastSocketError()}};
    }
    const int reuse = 1;
    ::setsockopt(asSocket(listener.handle_), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                 sizeof(reuse));
    if (address.family_ == AF_INET6) {
        const int onlyV6 = dualStack ? 0 : 1;
        if (::setsockopt(asSocket(listener.handle_), IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&onlyV6),
                         sizeof(onlyV6)) != 0) {
            return std::unexpected{SocketError{SocketError::Kind::ConfigureFailed, lastSocketError()}};
        }
    }
    if (::bind(asSocket(listener.handle_), reinterpret_cast<const sockaddr*>(address.storage_.data()),
               static_cast<socklen_type>(address.size_)) != 0) {
        return std::unexpected{SocketError{SocketError::Kind::BindFailed, lastSocketError()}};
    }
    if (::listen(asSocket(listener.handle_), backlog) != 0) {
        return std::unexpected{SocketError{SocketError::Kind::ListenFailed, lastSocketError()}};
    }
    return listener;
}

std::expected<TcpConnection, SocketError> TcpListener::accept() {
    sockaddr_storage peer{};
    auto length = static_cast<socklen_type>(sizeof(peer));
    while (true) {
        const auto raw = ::accept(asSocket(handle_), reinterpret_cast<sockaddr*>(&peer), &length);
#if defined(_WIN32)
        const std::int64_t clientHandle = raw == INVALID_SOCKET ? -1 : static_cast<std::int64_t>(raw);
#else
        const std::int64_t clientHandle = raw;
#endif
        if (clientHandle >= 0) {
            std::array<char, NI_MAXHOST> host{};
            const int code = ::getnameinfo(reinterpret_cast<const sockaddr*>(&peer), length, host.data(),
                                           static_cast<namelen_type>(host.size()), nullptr, 0, NI_NUMERICHOST);
            if (code != 0) {
                closeHandle(clientHandle);
                return std::unexpected{SocketError{SocketError::Kind::AcceptFailed, code}};
            }
            return TcpConnection{clientHandle, std::string{host.data()}};
        }
        const int error = lastSocketError();
        if (errorIsInterrupt(error)) {
            continue;
        }
        return std::unexpected{SocketError{errorWouldBlock(error) || retryableAcceptError(error)
                                               ? SocketError::Kind::WouldBlock
                                               : SocketError::Kind::AcceptFailed,
                                           error}};
    }
}

std::expected<std::uint16_t, SocketError> TcpListener::localPort() const {
    sockaddr_storage address{};
    auto length = static_cast<socklen_type>(sizeof(address));
    if (::getsockname(asSocket(handle_), reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return std::unexpected{SocketError{SocketError::Kind::BindFailed, lastSocketError()}};
    }
    if (address.ss_family == AF_INET6) {
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&address)->sin6_port);
    }
    return ntohs(reinterpret_cast<const sockaddr_in*>(&address)->sin_port);
}

} // namespace basalt::wire::io
