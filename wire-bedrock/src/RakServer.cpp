#include <basalt/wire/bedrock/RakServer.hpp>

#include <algorithm>
#include <chrono>
#include <random>
#include <utility>

namespace basalt::wire::bedrock {
namespace {

/// Bigger than any MTU a client will ask for, so a datagram is never read in halves.
constexpr std::size_t kReceiveBufferSize = 2048;

[[nodiscard]] std::uint64_t randomGuid() {
    std::random_device source;
    std::uniform_int_distribution<std::uint64_t> spread;
    std::mt19937_64 engine{(static_cast<std::uint64_t>(source()) << 32U) | source()};
    return spread(engine);
}

[[nodiscard]] bool isOfflineId(std::uint8_t id) noexcept {
    const auto message = static_cast<RakId>(id);
    return message == RakId::UnconnectedPing || message == RakId::UnconnectedPingOpenConnections ||
           message == RakId::OpenConnectionRequest1 || message == RakId::OpenConnectionRequest2;
}

} // namespace

RakServer::RakServer(RakServerConfig config) : config_(config) {
    if (config_.guid == 0) {
        config_.guid = randomGuid();
    }
    receiveBuffer_.resize(kReceiveBufferSize);
}

std::expected<void, io::SocketError> RakServer::start() {
    auto socket = io::UdpSocket::bind(config_.port);
    if (!socket) {
        return std::unexpected(socket.error());
    }
    socket_ = std::move(*socket);

    const auto bound = socket_.localPort();
    boundPort_ = bound ? *bound : config_.port;
    return {};
}

void RakServer::stop() {
    sessions_.clear();
    socket_.close();
}

RakSession* RakServer::session(const io::UdpEndpoint& peer) {
    const auto found = sessions_.find(peer);
    return found == sessions_.end() ? nullptr : found->second.get();
}

void RakServer::sendRaw(std::span<const std::uint8_t> bytes, const io::UdpEndpoint& to) {
    static_cast<void>(socket_.sendTo(bytes, to));
}

void RakServer::poll(std::uint64_t nowMs, int timeoutMs) {
    if (socket_.isClosed()) {
        return;
    }
    lastPollMs_ = nowMs;

    io::UdpEndpoint from{};
    int wait = timeoutMs;
    while (true) {
        auto received = socket_.receiveFrom(receiveBuffer_, from, wait);
        if (!received || !*received) {
            break;
        }
        wait = 0;

        const std::span<const std::uint8_t> datagram{receiveBuffer_.data(), received->value()};
        if (datagram.empty()) {
            continue;
        }

        if (RakSession* peer = session(from); peer != nullptr) {
            const bool wasConnected = peer->connected();
            messages_.clear();
            peer->receiveDatagram(datagram, nowMs, messages_);
            if (!wasConnected && peer->connected() && onConnect_) {
                onConnect_(*peer);
            }
            if (onMessage_) {
                for (const std::vector<std::uint8_t>& message : messages_) {
                    onMessage_(*peer, message);
                }
            }
            continue;
        }

        if (isOfflineId(datagram[0])) {
            handleOffline(datagram, from, nowMs);
        }
    }

    io::UdpEndpoint target{};
    const DatagramSink sink = [this, &target](std::span<const std::uint8_t> bytes) { sendRaw(bytes, target); };

    for (auto entry = sessions_.begin(); entry != sessions_.end();) {
        RakSession& peer = *entry->second;
        target = peer.peer();
        peer.flush(nowMs, sink);

        const bool timedOut = nowMs - peer.lastHeardMs() > kSessionTimeoutMs;
        if (peer.closed() || timedOut) {
            if (onDisconnect_) {
                onDisconnect_(peer);
            }
            entry = sessions_.erase(entry);
            continue;
        }
        ++entry;
    }
}

void RakServer::handleOffline(std::span<const std::uint8_t> datagram, const io::UdpEndpoint& from,
                              std::uint64_t nowMs) {
    static_cast<void>(nowMs);

    scratch_.clear();
    scratch_.writeBytes(datagram);
    const auto id = static_cast<RakId>(scratch_.readByte());

    switch (id) {
    case RakId::UnconnectedPing:
    case RakId::UnconnectedPingOpenConnections:
        onUnconnectedPing(scratch_, from);
        return;
    case RakId::OpenConnectionRequest1:
        onOpenConnectionRequest1(scratch_, datagram.size(), from);
        return;
    case RakId::OpenConnectionRequest2:
        onOpenConnectionRequest2(scratch_, from);
        return;
    default:
        return;
    }
}

void RakServer::onUnconnectedPing(io::ByteBuffer& body, const io::UdpEndpoint& from) {
    const std::int64_t pingTime = body.readLong();
    if (body.failed() || !hasMagic(body.readable())) {
        return;
    }

    const std::string advertisement = advertiser_ ? advertiser_() : std::string{};

    io::ByteBuffer out{advertisement.size() + 40};
    out.writeByte(static_cast<std::uint8_t>(RakId::UnconnectedPong));
    out.writeLong(pingTime);
    out.writeLong(static_cast<std::int64_t>(config_.guid));
    out.writeBytes(kMagic);
    out.writeShort(static_cast<std::int16_t>(advertisement.size()));
    out.writeBytes({reinterpret_cast<const std::uint8_t*>(advertisement.data()), advertisement.size()});
    sendRaw(out.readable(), from);
}

void RakServer::onOpenConnectionRequest1(io::ByteBuffer& body, std::size_t datagramSize, const io::UdpEndpoint& from) {
    if (!hasMagic(body.readable())) {
        return;
    }
    body.skip(kMagic.size());
    const std::uint8_t protocol = body.readByte();
    if (body.failed()) {
        return;
    }

    if (protocol != kRakNetProtocol) {
        io::ByteBuffer out{32};
        out.writeByte(static_cast<std::uint8_t>(RakId::IncompatibleProtocolVersion));
        out.writeByte(kRakNetProtocol);
        out.writeBytes(kMagic);
        out.writeLong(static_cast<std::int64_t>(config_.guid));
        sendRaw(out.readable(), from);
        return;
    }

    // The request is padded to the MTU the client wants to try, so its size is the answer.
    const auto wanted =
        static_cast<std::uint16_t>(std::clamp<std::size_t>(datagramSize + kIpv4Overhead, kMinimumMtu, kMaximumMtu));

    io::ByteBuffer out{32};
    out.writeByte(static_cast<std::uint8_t>(RakId::OpenConnectionReply1));
    out.writeBytes(kMagic);
    out.writeLong(static_cast<std::int64_t>(config_.guid));
    out.writeByte(0);
    out.writeShort(static_cast<std::int16_t>(wanted));
    sendRaw(out.readable(), from);
}

void RakServer::onOpenConnectionRequest2(io::ByteBuffer& body, const io::UdpEndpoint& from) {
    if (!hasMagic(body.readable())) {
        return;
    }
    body.skip(kMagic.size());
    static_cast<void>(readAddress(body));
    const auto mtu = static_cast<std::uint16_t>(body.readShort());
    const auto clientGuid = static_cast<std::uint64_t>(body.readLong());
    if (body.failed()) {
        return;
    }

    if (sessions_.contains(from)) {
        io::ByteBuffer out{32};
        out.writeByte(static_cast<std::uint8_t>(RakId::AlreadyConnected));
        out.writeBytes(kMagic);
        out.writeLong(static_cast<std::int64_t>(config_.guid));
        sendRaw(out.readable(), from);
        return;
    }
    if (sessions_.size() >= config_.maxSessions) {
        io::ByteBuffer out{32};
        out.writeByte(static_cast<std::uint8_t>(RakId::NoFreeIncomingConnections));
        out.writeBytes(kMagic);
        out.writeLong(static_cast<std::int64_t>(config_.guid));
        sendRaw(out.readable(), from);
        return;
    }

    const std::uint16_t agreed = std::clamp(mtu, kMinimumMtu, kMaximumMtu);
    sessions_.emplace(from, std::make_unique<RakSession>(from, agreed, clientGuid, lastPollMs_));

    io::ByteBuffer out{40};
    out.writeByte(static_cast<std::uint8_t>(RakId::OpenConnectionReply2));
    out.writeBytes(kMagic);
    out.writeLong(static_cast<std::int64_t>(config_.guid));
    writeAddress(out, from);
    out.writeShort(static_cast<std::int16_t>(agreed));
    out.writeByte(0);
    sendRaw(out.readable(), from);
}

} // namespace basalt::wire::bedrock
