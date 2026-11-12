#pragma once

#include <basalt/wire/bedrock/BedrockKeys.hpp>
#include <basalt/wire/bedrock/BedrockTransport.hpp>
#include <basalt/wire/io/UdpSocket.hpp>

#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace basalt::wire::bedrock {

struct NetherNetConfig {
    /// TCP port of the HTTP signaling, where a client asks how to connect.
    std::uint16_t signalingPort = 19132;

    /// UDP port of the WebRTC traffic; zero means the signaling port's number.
    std::uint16_t icePort = 0;

    /// The address clients are told to send to; empty means the one the signaling request reached.
    std::string advertiseHost;

    std::string domain = "basaltmc.org";

    /// The key the server proves itself with; a client trusts it on first use and then remembers it.
    std::shared_ptr<const EcKeyPair> identityKey;

    std::size_t maxConnections = 512;

    /// Refuse an offer that does not prove which key signed it.
    bool requireIdentity = false;
};

/// What `GET /v1/join` tells a client's server list.
struct NetherNetStatus {
    std::string name;
    std::string version;
    std::string level;
    std::uint32_t protocol = 0;
    std::uint32_t players = 0;
    std::uint32_t maxPlayers = 0;
    std::uint8_t gameType = 0;
};

/// One connected client; `sendGame` may be called from any thread.
class NetherNetLink final : public BedrockTransport {
public:
    using Sender = std::function<void(std::span<const std::uint8_t>)>;

    NetherNetLink(Sender sender, std::function<void()> closer, io::UdpEndpoint peer,
                  std::vector<std::uint8_t> identityKey);

    void sendGame(std::span<const std::uint8_t> payload) override { sender_(payload); }

    [[nodiscard]] bool protectsBytes() const noexcept override { return true; }

    [[nodiscard]] const io::UdpEndpoint& peer() const noexcept { return peer_; }

    /// The DER key the connection proved it holds, or empty when the client offered no proof.
    [[nodiscard]] const std::vector<std::uint8_t>& identityKey() const noexcept { return identityKey_; }

    [[nodiscard]] bool closed() const noexcept { return closed_.load(); }

    /// Lets what is queued go out, then drops the connection.
    void close() { closer_(); }

    void markClosed() noexcept { closed_.store(true); }

private:
    Sender sender_;
    std::function<void()> closer_;
    io::UdpEndpoint peer_;
    std::vector<std::uint8_t> identityKey_;
    std::atomic<bool> closed_{false};
};

/// NetherNet as a dedicated server speaks it: HTTP signaling over TCP, then WebRTC data channels over UDP.
class NetherNetServer {
public:
    using StatusProvider = std::function<NetherNetStatus()>;
    using LinkHandler = std::function<void(const std::shared_ptr<NetherNetLink>&)>;
    using MessageHandler = std::function<void(const std::shared_ptr<NetherNetLink>&, std::span<const std::uint8_t>)>;

    explicit NetherNetServer(NetherNetConfig config);
    ~NetherNetServer();
    NetherNetServer(const NetherNetServer&) = delete;
    NetherNetServer& operator=(const NetherNetServer&) = delete;

    /// Runs on the network thread, so it must not touch anything the owner's thread owns.
    void onStatus(StatusProvider provider);

    void onConnect(LinkHandler handler);

    void onMessage(MessageHandler handler);

    void onDisconnect(LinkHandler handler);

    [[nodiscard]] std::expected<void, std::string> start();

    void stop();

    /// Hands what the network thread collected to the handlers, on the caller's thread.
    void drain();

    [[nodiscard]] std::uint16_t signalingPort() const noexcept;

    [[nodiscard]] std::uint16_t icePort() const noexcept;

    [[nodiscard]] std::size_t connectionCount() const noexcept;

private:
    struct Impl;

    std::unique_ptr<Impl> impl_;
};

} // namespace basalt::wire::bedrock
