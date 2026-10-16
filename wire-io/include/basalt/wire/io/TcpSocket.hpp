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

/// Resolving is synchronous and belongs on a worker, never an I/O owner; use resolveDns for a deadline.
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

    /// Await writable readiness, then call finishConnect before reading or writing.
    [[nodiscard]] static std::expected<TcpConnection, SocketError> connectNonBlocking(std::string_view host,
                                                                                      std::uint16_t port);
    [[nodiscard]] static std::expected<TcpConnection, SocketError> connectNonBlocking(const TcpAddress& address);

    [[nodiscard]] std::expected<void, SocketError> setNonBlocking(bool enabled = true);

    [[nodiscard]] std::expected<bool, SocketError> finishConnect();

    [[nodiscard]] const std::string& remoteAddress() const noexcept { return remoteAddress_; }

    /// Zero unless the connection was accepted.
    [[nodiscard]] std::uint16_t remotePort() const noexcept { return remotePort_; }

    [[nodiscard]] std::int64_t nativeHandle() const noexcept { return handle_; }

    [[nodiscard]] bool isClosed() const noexcept { return handle_ < 0; }

    [[nodiscard]] std::expected<std::size_t, SocketError> read(std::span<std::uint8_t> into);

    [[nodiscard]] std::expected<void, SocketError> write(std::span<const std::uint8_t> bytes);

    /// WouldBlock means no progress, never end of stream; unlike write, a disconnected peer is an error.
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
    std::uint16_t remotePort_ = 0;
};

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

    /// Asks who the peer is immediately: this is the last moment the answer is guaranteed to exist.
    [[nodiscard]] std::expected<TcpConnection, SocketError> accept();

    [[nodiscard]] std::expected<std::uint16_t, SocketError> localPort() const;

    [[nodiscard]] std::int64_t nativeHandle() const noexcept { return handle_; }

    void close() noexcept;

private:
    explicit TcpListener(std::int64_t handle) : handle_(handle) {}

    std::int64_t handle_ = -1;
};

[[nodiscard]] std::optional<std::uint32_t> parseIpv4(std::string_view address) noexcept;

[[nodiscard]] std::string formatIpv4(std::uint32_t networkOrder);

} // namespace basalt::wire::io
