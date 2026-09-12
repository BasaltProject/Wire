#pragma once

#include <basalt/wire/bedrock/RakSession.hpp>
#include <basalt/wire/io/UdpSocket.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace basalt::wire::bedrock {

/// What a server needs to know before it can answer a ping.
struct RakServerConfig {
    std::uint16_t port = 19132;

    /// The server's own identity, which the client checks across the handshake. Zero asks for a
    /// random one, which is what a server wants: two servers with the same guid confuse a client
    /// that has both in its list.
    std::uint64_t guid = 0;

    std::size_t maxSessions = 512;
};

/// The RakNet server: one UDP socket, the unconnected handshake, and a session per peer. It owns no
/// thread and knows nothing about Bedrock; poll() is called from whoever does.
class RakServer {
public:
    /// The line a client reads in its server list. Asked for on every ping because the player count
    /// moves.
    using Advertiser = std::function<std::string()>;

    using MessageHandler = std::function<void(RakSession&, std::span<const std::uint8_t>)>;
    using SessionHandler = std::function<void(RakSession&)>;

    explicit RakServer(RakServerConfig config);

    [[nodiscard]] std::expected<void, io::SocketError> start();

    void stop();

    [[nodiscard]] bool running() const noexcept { return !socket_.isClosed(); }

    [[nodiscard]] std::uint16_t port() const noexcept { return boundPort_; }

    [[nodiscard]] std::uint64_t guid() const noexcept { return config_.guid; }

    [[nodiscard]] std::size_t sessionCount() const noexcept { return sessions_.size(); }

    void onAdvertise(Advertiser advertiser) { advertiser_ = std::move(advertiser); }

    void onMessage(MessageHandler handler) { onMessage_ = std::move(handler); }

    void onConnect(SessionHandler handler) { onConnect_ = std::move(handler); }

    void onDisconnect(SessionHandler handler) { onDisconnect_ = std::move(handler); }

    /// Reads whatever has arrived, answers it, and flushes every session. Blocks up to timeoutMs
    /// waiting for the first datagram, so a caller can use it as its own tick.
    void poll(std::uint64_t nowMs, int timeoutMs);

    /// Queues a game payload to one peer, for a caller on another thread. The bytes go out on the
    /// next poll.
    [[nodiscard]] RakSession* session(const io::UdpEndpoint& peer);

private:
    void handleOffline(std::span<const std::uint8_t> datagram, const io::UdpEndpoint& from, std::uint64_t nowMs);
    void onUnconnectedPing(io::ByteBuffer& body, const io::UdpEndpoint& from);
    void onOpenConnectionRequest1(io::ByteBuffer& body, std::size_t datagramSize, const io::UdpEndpoint& from);
    void onOpenConnectionRequest2(io::ByteBuffer& body, const io::UdpEndpoint& from);
    void sendRaw(std::span<const std::uint8_t> bytes, const io::UdpEndpoint& to);

    RakServerConfig config_;
    io::UdpSocket socket_;
    std::uint16_t boundPort_ = 0;
    std::uint64_t lastPollMs_ = 0;

    Advertiser advertiser_;
    MessageHandler onMessage_;
    SessionHandler onConnect_;
    SessionHandler onDisconnect_;

    std::unordered_map<io::UdpEndpoint, std::unique_ptr<RakSession>> sessions_;
    std::vector<std::uint8_t> receiveBuffer_;
    std::vector<std::vector<std::uint8_t>> messages_;
    io::ByteBuffer scratch_;
};

} // namespace basalt::wire::bedrock
