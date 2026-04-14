#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::io {

/// Why a socket call failed. The raw platform code is kept alongside: POSIX and Winsock number
/// their errors differently and a shared translation table would only lose detail.
struct SocketError {
    enum class Kind : std::uint8_t {
        CreateFailed,
        BindFailed,
        ListenFailed,
        AcceptFailed,
        ConnectFailed,
        InvalidAddress,
        SendFailed,
        ReceiveFailed,
        ConfigureFailed,
        WouldBlock,
        ResolveFailed,
    };

    Kind kind = Kind::CreateFailed;
    int platformCode = 0;
};

/// A resolved endpoint. Resolving is synchronous and belongs on a worker, never an I/O owner.
/// Literal parsing does not perform DNS; both IPv4 and IPv6 (including scope IDs) are supported.
class TcpAddress {
public:
    [[nodiscard]] static std::expected<TcpAddress, SocketError> literal(std::string_view host, std::uint16_t port);
    [[nodiscard]] static std::expected<std::vector<TcpAddress>, SocketError> resolve(std::string_view host,
                                                                                     std::uint16_t port);
    [[nodiscard]] std::string host() const;

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

private:
    friend class TcpConnection;
    friend class TcpListener;
    [[nodiscard]] static std::expected<std::vector<TcpAddress>, SocketError> lookup(std::string_view host,
                                                                                    std::uint16_t port, bool numeric);
    alignas(std::max_align_t) std::array<std::uint8_t, 128> storage_{};
    std::uint32_t size_ = 0;
    int family_ = 0;
    std::uint16_t port_ = 0;
};

/// A connected IPv4 or IPv6 TCP socket. connect uses blocking I/O; connectNonBlocking and
/// setNonBlocking expose partial progress for readiness-based consumers.
///
/// The handle is a 64-bit integer so POSIX's `int` and Winsock's `SOCKET` share one type: reading
/// Winsock's all-ones INVALID_SOCKET as a signed value lands on -1, exactly where POSIX puts it, so
/// the same "negative means invalid" check is right on both.
class TcpConnection {
public:
    TcpConnection() = default;
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;
    ~TcpConnection();

    [[nodiscard]] static std::expected<TcpConnection, SocketError> connect(std::string_view host, std::uint16_t port);
    [[nodiscard]] static std::expected<TcpConnection, SocketError> connect(const TcpAddress& address);

    /// Starts a literal-address connection without waiting for the handshake. Await writable readiness,
    /// then call finishConnect before reading or writing. The host must be a resolved literal.
    [[nodiscard]] static std::expected<TcpConnection, SocketError> connectNonBlocking(std::string_view host,
                                                                                      std::uint16_t port);
    [[nodiscard]] static std::expected<TcpConnection, SocketError> connectNonBlocking(const TcpAddress& address);

    [[nodiscard]] std::expected<void, SocketError> setNonBlocking(bool enabled = true);

    [[nodiscard]] std::expected<bool, SocketError> finishConnect();

    /// The peer's literal address, empty when the socket was not accepted from a listener.
    [[nodiscard]] const std::string& remoteAddress() const noexcept { return remoteAddress_; }

    [[nodiscard]] std::int64_t nativeHandle() const noexcept { return handle_; }

    [[nodiscard]] bool isClosed() const noexcept { return handle_ < 0; }

    /// Returns 0 at end of stream, including when the peer reset the connection instead of closing
    /// it politely. A client that pings the server list and yanks the socket the moment it has its
    /// answer is doing something ordinary, not failing, and that arrives as a reset.
    [[nodiscard]] std::expected<std::size_t, SocketError> read(std::span<std::uint8_t> into);

    /// Writes everything, or stops quietly once the peer is gone, for the same reason read returns 0.
    [[nodiscard]] std::expected<void, SocketError> write(std::span<const std::uint8_t> bytes);

    /// Sends at most one chunk; WouldBlock means no progress, never end of stream. Unlike write,
    /// this reports a disconnected peer as an error instead of claiming all bytes were sent.
    [[nodiscard]] std::expected<std::size_t, SocketError> writeSome(std::span<const std::uint8_t> bytes);

    void close() noexcept;

private:
    friend class TcpListener;
    [[nodiscard]] static std::expected<TcpConnection, SocketError> connectAddress(const TcpAddress& address,
                                                                                  bool nonblocking);

    explicit TcpConnection(std::int64_t handle, std::string remoteAddress = {})
        : handle_(handle), remoteAddress_(std::move(remoteAddress)) {}

    std::int64_t handle_ = -1;
    std::string remoteAddress_;
};

/// A listening TCP socket. The port-only overload binds all IPv4 interfaces.
class TcpListener {
public:
    TcpListener() = default;
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;
    TcpListener(TcpListener&& other) noexcept;
    TcpListener& operator=(TcpListener&& other) noexcept;
    ~TcpListener();

    [[nodiscard]] static std::expected<TcpListener, SocketError> bind(std::uint16_t port, int backlog = 128);
    [[nodiscard]] static std::expected<TcpListener, SocketError> bind(const TcpAddress& address, int backlog = 128,
                                                                      bool dualStack = false);

    [[nodiscard]] std::expected<void, SocketError> setNonBlocking(bool enabled = true);

    /// Accepts one connection and asks who it is from straight away, because this is the last moment
    /// the answer is guaranteed to still be there.
    [[nodiscard]] std::expected<TcpConnection, SocketError> accept();

    /// The port actually bound, which is what port 0 was resolved to.
    [[nodiscard]] std::expected<std::uint16_t, SocketError> localPort() const;

    [[nodiscard]] std::int64_t nativeHandle() const noexcept { return handle_; }

    void close() noexcept;

private:
    explicit TcpListener(std::int64_t handle) : handle_(handle) {}

    std::int64_t handle_ = -1;
};

/// Parses a literal IPv4 address into its four octets in network order.
[[nodiscard]] std::optional<std::uint32_t> parseIpv4(std::string_view address) noexcept;

[[nodiscard]] std::string formatIpv4(std::uint32_t networkOrder);

} // namespace basalt::wire::io
