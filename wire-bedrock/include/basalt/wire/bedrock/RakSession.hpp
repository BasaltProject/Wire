#pragma once

#include <basalt/wire/bedrock/RakFrame.hpp>
#include <basalt/wire/bedrock/RakNet.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/io/UdpSocket.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace basalt::wire::bedrock {

/// Where a datagram goes once the session has built it. A function and not a socket because a test
/// wants the bytes and a server wants them sent.
using DatagramSink = std::function<void(std::span<const std::uint8_t>)>;

/// Sixteen ordering channels, which is what RakNet defines. Bedrock uses one of them.
inline constexpr std::size_t kOrderChannels = 16;

/// How long a reliable datagram waits for its acknowledgement before going out again.
inline constexpr std::uint64_t kResendTimeoutMs = 500;

/// Nothing to say for this long and the session says something anyway, so the peer does not decide
/// we are gone.
inline constexpr std::uint64_t kKeepAliveMs = 2500;

/// One peer's half of RakNet: acknowledgement, ordering, splitting and the handshake that turns an
/// address into a connection. It never touches a socket; the datagrams it builds go to a sink.
class RakSession {
public:
    /// What the transport did for this peer, kept so a session that goes quiet can be told apart
    /// from one that said goodbye. A client that stops acknowledging halfway through a burst is a
    /// transport problem; one that acknowledges everything and then leaves read something it did
    /// not like.
    struct Stats {
        std::uint64_t datagramsSent = 0;
        std::uint64_t datagramsReceived = 0;
        std::uint64_t datagramsResent = 0;
        std::uint64_t datagramsAcknowledged = 0;
        std::uint64_t datagramsAskedAgain = 0;
        std::uint64_t splitMessages = 0;
        std::uint64_t splitParts = 0;
    };

    RakSession(io::UdpEndpoint peer, std::uint16_t mtu, std::uint64_t clientGuid, std::uint64_t nowMs);

    [[nodiscard]] const io::UdpEndpoint& peer() const noexcept { return peer_; }

    [[nodiscard]] std::uint64_t clientGuid() const noexcept { return clientGuid_; }

    [[nodiscard]] std::uint16_t mtu() const noexcept { return mtu_; }

    /// True once the peer has finished the RakNet handshake and its messages are the game's.
    [[nodiscard]] bool connected() const noexcept { return connected_; }

    [[nodiscard]] bool closed() const noexcept { return closed_; }

    /// True when the peer sent a disconnection notification rather than simply going quiet.
    [[nodiscard]] bool saidGoodbye() const noexcept { return saidGoodbye_; }

    [[nodiscard]] const Stats& stats() const noexcept { return stats_; }

    [[nodiscard]] std::uint64_t lastHeardMs() const noexcept { return lastHeardMs_; }

    /// Takes one datagram off the socket. Game payloads that came out whole and in order are
    /// appended to messages, with the 0xfe already stripped; everything else the transport answers
    /// by itself.
    void receiveDatagram(std::span<const std::uint8_t> datagram, std::uint64_t nowMs,
                         std::vector<std::vector<std::uint8_t>>& messages);

    /// Queues a game payload. The 0xfe goes on here, so callers pass the batch and nothing else.
    void sendGame(std::span<const std::uint8_t> payload);

    /// Queues a raw RakNet message, id byte included.
    void sendMessage(std::span<const std::uint8_t> message, Reliability reliability, std::uint8_t channel = 0);

    /// Everything that has to go out now: acknowledgements, resends and queued frames.
    void flush(std::uint64_t nowMs, const DatagramSink& sink);

    /// Tells the peer we are done and marks the session closed.
    void disconnect(std::uint64_t nowMs, const DatagramSink& sink);

private:
    struct SentDatagram {
        std::vector<RakFrame> frames;
        std::uint64_t sentAtMs = 0;
    };

    struct SplitBuffer {
        std::uint32_t partCount = 0;
        std::uint32_t received = 0;
        std::uint64_t startedMs = 0;
        std::vector<std::vector<std::uint8_t>> parts;
    };

    struct OrderChannel {
        std::uint32_t expectedIndex = 0;
        std::uint32_t highestSequence = 0;
        bool anySequence = false;
        std::map<std::uint32_t, std::vector<std::uint8_t>> held;
    };

    void handleFrame(RakFrame& frame, std::uint64_t nowMs, std::vector<std::vector<std::uint8_t>>& messages);
    void deliver(std::vector<std::uint8_t>&& message, std::vector<std::vector<std::uint8_t>>& messages);
    void handleMessage(std::vector<std::uint8_t>&& message, std::uint64_t nowMs,
                       std::vector<std::vector<std::uint8_t>>& messages);
    void onConnectionRequest(io::ByteBuffer& body, std::uint64_t nowMs);
    void onConnectedPing(io::ByteBuffer& body, std::uint64_t nowMs);

    void queueFrames(std::span<const std::uint8_t> message, Reliability reliability, std::uint8_t channel);
    void sendRanges(std::uint8_t flags, std::vector<std::uint32_t>& sequences, const DatagramSink& sink);
    void removeAcknowledged(std::span<const SequenceRange> ranges);
    void requeueNaked(std::span<const SequenceRange> ranges);

    [[nodiscard]] std::size_t maxPayload() const noexcept;

    io::UdpEndpoint peer_;
    std::uint16_t mtu_ = kMinimumMtu;
    std::uint64_t clientGuid_ = 0;
    bool connected_ = false;
    bool closed_ = false;
    bool saidGoodbye_ = false;
    Stats stats_;
    std::uint64_t lastHeardMs_ = 0;
    std::uint64_t lastSentMs_ = 0;

    std::uint32_t expectedSequence_ = 0;
    std::vector<std::uint32_t> ackQueue_;
    std::vector<std::uint32_t> nakQueue_;
    std::unordered_set<std::uint32_t> seenReliable_;
    std::uint32_t highestReliable_ = 0;
    std::array<OrderChannel, kOrderChannels> channels_;
    std::unordered_map<std::uint16_t, SplitBuffer> splits_;

    std::uint32_t nextDatagramSequence_ = 0;
    std::uint32_t nextReliableIndex_ = 0;
    std::array<std::uint32_t, kOrderChannels> nextOrderIndex_{};
    std::array<std::uint32_t, kOrderChannels> nextSequenceIndex_{};
    std::uint16_t nextSplitId_ = 0;
    std::vector<RakFrame> outgoing_;
    std::map<std::uint32_t, SentDatagram> unacknowledged_;

    io::ByteBuffer inbound_;
    io::ByteBuffer reader_;
    io::ByteBuffer scratch_;
};

} // namespace basalt::wire::bedrock
