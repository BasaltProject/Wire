#include "NetherNetServerSupport.hpp"

namespace basalt::wire::bedrock {
NetherNetLink::NetherNetLink(Sender sender, std::function<void()> closer, io::UdpEndpoint peer,
                             std::vector<std::uint8_t> identityKey)
    : sender_(std::move(sender)), closer_(std::move(closer)), peer_(peer), identityKey_(std::move(identityKey)) {}

struct NetherNetServer::Impl {
    NetherNetConfig config;
    std::unique_ptr<nethernet::DtlsCredentials> credentials;
    io::UdpSocket udp;
    io::TcpListener listener;
    std::shared_ptr<io::EventLoop> loop = std::make_shared<io::EventLoop>();
    std::jthread thread;
    std::atomic<bool> running{false};
    std::uint16_t signalingPort = 0;
    std::uint16_t icePort = 0;
    std::atomic<std::size_t> connections{0};

    StatusProvider status;
    SignalGate signal;
    AbandonHandler abandoned;
    std::unordered_map<std::string, std::uint32_t> httpPerAddress;
    LinkHandler connected;
    MessageHandler message;
    LinkHandler disconnected;

    std::mutex eventLock;
    std::vector<LinkEvent> events;

    std::vector<std::unique_ptr<Peer>> peers;
    std::unordered_map<std::string, Peer*> byUfrag;
    std::unordered_map<io::UdpEndpoint, Peer*> byEndpoint;
    std::unordered_map<std::uint64_t, HttpConnection> http;
    std::uint64_t nextToken = kFirstHttpToken;
    std::uint64_t nextPeerId = 1;
    std::vector<std::uint8_t> datagram = std::vector<std::uint8_t>(2048);

    void run();
    void acceptConnections();
    void readHttp(std::uint64_t token);
    void writeHttp(std::uint64_t token);
    void closeHttp(std::uint64_t token);
    void respond(std::uint64_t token, const nethernet::HttpRequest& request);
    std::string negotiate(HttpConnection& connection, const nethernet::HttpRequest& request, int& statusCode,
                          std::string& contentType);
    void readUdp();
    void handleStun(std::span<const std::uint8_t> bytes, const io::UdpEndpoint& from);
    void servicePeers(Clock::time_point now);
    void serviceSctp(Peer& peer);
    void flushOutbox(Peer& peer, Clock::time_point now);
    void deliverMessage(Peer& peer, std::uint16_t stream, std::vector<std::uint8_t> data);
    void announce(Peer& peer);
    void removePeer(Peer& peer);
    void push(LinkEvent event);

    void wake() const { (void)loop->wake(); }
};

void NetherNetServer::Impl::push(LinkEvent event) {
    const std::lock_guard<std::mutex> guard{eventLock};
    events.push_back(std::move(event));
}

void NetherNetServer::Impl::run() {
    while (running.load()) {
        const int timeout = peers.empty() && http.empty() ? 1000 : 20;
        const auto ready = loop->wait(timeout);
        if (ready) {
            for (const io::IoReady& item : *ready) {
                if (item.token == kListenerToken) {
                    acceptConnections();
                } else if (item.token == kUdpToken) {
                    readUdp();
                } else if (item.writable) {
                    writeHttp(item.token);
                } else if (item.readable || item.closed) {
                    readHttp(item.token);
                }
            }
        }

        const Clock::time_point now = Clock::now();
        std::vector<std::uint64_t> expired;
        for (const auto& [token, connection] : http) {
            if (now > connection.deadline) {
                expired.push_back(token);
            }
        }
        for (const std::uint64_t token : expired) {
            closeHttp(token);
        }
        servicePeers(now);
    }

    abandoned = nullptr;
    for (const auto& peer : peers) {
        peer->dead = true;
        removePeer(*peer);
    }
    peers.clear();
    for (auto& [token, connection] : http) {
        loop->unwatch(connection.socket.nativeHandle());
    }
    http.clear();
}

void NetherNetServer::Impl::acceptConnections() {
    while (true) {
        auto accepted = listener.accept();
        if (!accepted) {
            return;
        }
        if (!accepted->setNonBlocking()) {
            continue;
        }
        if (http.size() >= kMaxHttpConnections || httpPerAddress[accepted->remoteAddress()] >= kMaxHttpPerAddress) {
            continue;
        }
        const std::uint64_t token = nextToken++;
        const std::int64_t handle = accepted->nativeHandle();
        ++httpPerAddress[accepted->remoteAddress()];
        HttpConnection& connection = http[token];
        connection.socket = std::move(*accepted);
        connection.deadline = Clock::now() + kHttpTimeout;
        if (!loop->watch(handle, io::IoInterest{true, false}, token)) {
            closeHttp(token);
        }
    }
}

void NetherNetServer::Impl::closeHttp(std::uint64_t token) {
    const auto found = http.find(token);
    if (found == http.end()) {
        return;
    }
    loop->unwatch(found->second.socket.nativeHandle());
    if (const auto count = httpPerAddress.find(found->second.socket.remoteAddress());
        count != httpPerAddress.end() && --count->second == 0) {
        httpPerAddress.erase(count);
    }
    http.erase(found);
}

void NetherNetServer::Impl::readHttp(std::uint64_t token) {
    const auto found = http.find(token);
    if (found == http.end() || found->second.responding) {
        return;
    }
    HttpConnection& connection = found->second;
    std::array<std::uint8_t, 4096> buffer{};
    while (true) {
        const auto count = connection.socket.read(buffer);
        if (!count) {
            if (count.error().kind == io::SocketError::Kind::WouldBlock) {
                break;
            }
            closeHttp(token);
            return;
        }
        if (*count == 0) {
            closeHttp(token);
            return;
        }
        connection.input.append(reinterpret_cast<const char*>(buffer.data()), *count);
        if (connection.input.size() > nethernet::kMaxHeaderBytes + nethernet::kMaxBodyBytes) {
            closeHttp(token);
            return;
        }
    }

    if (!connection.input.empty() && (connection.input.front() < 'A' || connection.input.front() > 'Z')) {
        closeHttp(token);
        return;
    }
    nethernet::HttpRequest request;
    switch (nethernet::parseHttpRequest(connection.input, request)) {
    case nethernet::HttpParse::Incomplete:
        return;
    case nethernet::HttpParse::Complete:
        respond(token, request);
        return;
    case nethernet::HttpParse::TooLarge:
        connection.output = nethernet::httpResponse(413, "text/plain", "Request too large");
        break;
    case nethernet::HttpParse::Bad:
        connection.output = nethernet::httpResponse(400, "text/plain", "Bad request");
        break;
    }
    connection.responding = true;
    writeHttp(token);
}

void NetherNetServer::Impl::writeHttp(std::uint64_t token) {
    const auto found = http.find(token);
    if (found == http.end()) {
        return;
    }
    HttpConnection& connection = found->second;
    while (!connection.output.empty()) {
        const auto written = connection.socket.writeSome(
            {reinterpret_cast<const std::uint8_t*>(connection.output.data()), connection.output.size()});
        if (!written) {
            if (written.error().kind == io::SocketError::Kind::WouldBlock) {
                (void)loop->watch(connection.socket.nativeHandle(), io::IoInterest{false, true}, token);
                return;
            }
            closeHttp(token);
            return;
        }
        connection.output.erase(0, *written);
    }
    closeHttp(token);
}

void NetherNetServer::Impl::respond(std::uint64_t token, const nethernet::HttpRequest& request) {
    HttpConnection& connection = http.at(token);
    int statusCode = 404;
    std::string contentType = "text/plain";
    std::string body = "Unknown NetherNet endpoint";

    constexpr std::string_view kJoin = "/v1/join";
    const std::string path = request.path.substr(0, request.path.find('?'));
    const bool offer = request.method == "POST" && path.starts_with("/v1/join/");
    if (signal && (offer || request.method == "GET") && !signal(connection.socket.remoteAddress(), offer)) {
        statusCode = 429;
        body = "Too many requests";
    } else if (request.method == "GET" && path == kJoin) {
        statusCode = 200;
        contentType = "application/json";
        body = statusJson(status ? status() : NetherNetStatus{});
    } else if (offer) {
        body = negotiate(connection, request, statusCode, contentType);
    }

    connection.output = nethernet::httpResponse(statusCode, contentType, body);
    connection.responding = true;
    writeHttp(token);
}

std::string NetherNetServer::Impl::negotiate(HttpConnection& connection, const nethernet::HttpRequest& request,
                                             int& statusCode, std::string& contentType) {
    statusCode = 400;
    contentType = "text/plain";

    const std::string_view networkId = std::string_view{request.path}.substr(
        std::string_view{"/v1/join/"}.size(), request.path.find('?') - std::string_view{"/v1/join/"}.size());
    std::uint64_t parsedId = 0;
    const auto [pointer, error] = std::from_chars(networkId.data(), networkId.data() + networkId.size(), parsedId);
    if (networkId.empty() || error != std::errc{} || pointer != networkId.data() + networkId.size()) {
        return "Network ID must be uint64";
    }
    if (request.body.empty()) {
        return "Missing SDP offer";
    }

    const auto offer = parseNetherNetOffer(request.body);
    if (!offer) {
        return offer.error();
    }

    std::vector<std::uint8_t> identityKey;
    if (!offer->identity.empty()) {
        const auto identity =
            verifyNetherNetOfferIdentity(offer->identity, offer->fingerprintAlgorithm, offer->fingerprintDigest);
        if (!identity) {
            return identity.error();
        }
        identityKey = identity->publicKeyDer;
    } else if (config.requireIdentity) {
        statusCode = 403;
        return "An identity is required";
    }

    const auto negotiating = static_cast<std::size_t>(std::ranges::count_if(
        peers, [](const std::unique_ptr<Peer>& peer) { return !peer->announced && !peer->dead; }));
    if (peers.size() >= config.maxConnections || negotiating >= config.maxNegotiating) {
        statusCode = 503;
        return "Service unavailable";
    }

    std::string host = config.advertiseHost.empty() ? connection.socket.localAddress() : config.advertiseHost;
    if (host.starts_with("::ffff:")) {
        host.erase(0, 7);
    }
    if (host.empty() || host.find(':') != std::string::npos) {
        statusCode = 500;
        return "No IPv4 address to offer";
    }

    NetherNetAnswer answer;
    answer.iceUfrag = randomText(8);
    answer.icePwd = randomText(24);
    answer.fingerprintDigest = credentials->fingerprint();
    answer.host = host;
    answer.port = icePort;
    if (config.identityKey != nullptr) {
        const auto identity =
            buildNetherNetAnswerIdentity(*config.identityKey, config.domain, answer.fingerprintDigest);
        if (!identity) {
            statusCode = 500;
            return "Could not sign the answer";
        }
        answer.identity = *identity;
    }

    auto peer = std::make_unique<Peer>();
    peer->id = nextPeerId++;
    peer->ufrag = answer.iceUfrag;
    peer->pwd = answer.icePwd;
    peer->identityKey = std::move(identityKey);
    peer->created = Clock::now();
    peer->origin = connection.socket.remoteAddress();
    peer->lastHeard = peer->created;
    Peer* raw = peer.get();
    peer->dtls = std::make_unique<nethernet::DtlsSession>(*credentials, offer->fingerprintDigest,
                                                          [this, raw](std::span<const std::uint8_t> bytes) {
                                                              if (raw->iceReady) {
                                                                  (void)udp.sendTo(bytes, raw->endpoint);
                                                              }
                                                          });
    byUfrag[peer->ufrag] = raw;
    peers.push_back(std::move(peer));

    statusCode = 200;
    contentType = "application/sdp";
    return buildNetherNetAnswer(answer, raw->id);
}

void NetherNetServer::Impl::readUdp() {
    while (true) {
        io::UdpEndpoint from;
        const auto count = udp.receiveFrom(datagram, from, 0);
        if (!count || !*count) {
            return;
        }
        const std::span<const std::uint8_t> bytes{datagram.data(), **count};
        if (looksLikeStun(bytes)) {
            handleStun(bytes, from);
            continue;
        }
        if (const auto found = byEndpoint.find(from); found != byEndpoint.end()) {
            found->second->lastHeard = Clock::now();
            found->second->dtls->feed(bytes);
        }
    }
}

void NetherNetServer::Impl::handleStun(std::span<const std::uint8_t> bytes, const io::UdpEndpoint& from) {
    Peer* peer = nullptr;
    if (const auto found = byEndpoint.find(from); found != byEndpoint.end()) {
        peer = found->second;
    } else if (const auto username = stunUsername(bytes); username) {
        const std::size_t colon = username->find(':');
        if (const auto owner = byUfrag.find(username->substr(0, colon)); owner != byUfrag.end()) {
            peer = owner->second;
        }
    }
    if (peer == nullptr) {
        return;
    }
    const auto request = parseStunBindingRequest(bytes, peer->pwd);
    if (!request) {
        return;
    }

    if (!peer->iceReady || !(peer->endpoint == from)) {
        if (peer->iceReady) {
            byEndpoint.erase(peer->endpoint);
        }
        peer->endpoint = from;
        peer->iceReady = true;
        byEndpoint[from] = peer;
    }
    peer->lastHeard = Clock::now();
    const std::vector<std::uint8_t> reply = buildStunBindingSuccess(request->transaction, from, peer->pwd);
    (void)udp.sendTo(reply, from);
}

void NetherNetServer::Impl::servicePeers(Clock::time_point now) {
    for (std::size_t index = 0; index < peers.size();) {
        Peer& peer = *peers[index];
        if (!peer.dead) {
            const bool stale = (peer.iceReady ? now - peer.lastHeard > kSilenceTimeout : false) ||
                               (!peer.announced && now - peer.created > kNegotiationTimeout);
            if (stale) {
                peer.dead = true;
            }
        }
        if (!peer.dead && peer.iceReady) {
            std::vector<std::vector<std::uint8_t>> received;
            peer.dtls->pump(received);
            if (peer.dtls->failed()) {
                peer.dead = true;
            } else if (peer.dtls->established()) {
                if (peer.sctp == nullptr) {
                    peer.sctp = std::make_unique<nethernet::SctpEngine>([this] { wake(); });
                    if (!peer.sctp->connect()) {
                        peer.dead = true;
                    }
                }
                if (peer.sctp != nullptr) {
                    for (const auto& packet : received) {
                        peer.sctp->input(packet);
                    }
                    serviceSctp(peer);
                    flushOutbox(peer, now);
                }
            }
        }
        if (peer.dead) {
            removePeer(peer);
            peers.erase(peers.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        ++index;
    }
}

void NetherNetServer::Impl::serviceSctp(Peer& peer) {
    std::vector<nethernet::SctpEvent> taken;
    peer.sctp->takeEvents(taken);
    for (nethernet::SctpEvent& event : taken) {
        switch (event.kind) {
        case nethernet::SctpEvent::Kind::Established:
            peer.sctpUp = true;
            break;
        case nethernet::SctpEvent::Kind::Closed:
            peer.dead = true;
            break;
        case nethernet::SctpEvent::Kind::ChannelOpened:
            if (event.label == kReliableLabel && event.ordered) {
                peer.reliable = event.stream;
            } else if (event.label == kUnreliableLabel && !event.ordered) {
                peer.unreliable = event.stream;
            } else {
                peer.dead = true;
                break;
            }
            (void)peer.sctp->acknowledge(event.stream);
            break;
        case nethernet::SctpEvent::Kind::Message:
            deliverMessage(peer, event.stream, std::move(event.data));
            break;
        }
    }

    std::vector<std::vector<std::uint8_t>> packets;
    peer.sctp->takeOutput(packets);
    for (const auto& packet : packets) {
        (void)peer.dtls->send(packet);
    }
    if (!peer.announced && peer.reliable && peer.unreliable && !peer.dead) {
        announce(peer);
    }
}

void NetherNetServer::Impl::deliverMessage(Peer& peer, std::uint16_t stream, std::vector<std::uint8_t> data) {
    if (data.size() < 2 || !peer.announced) {
        if (data.size() < 2) {
            peer.dead = true;
        }
        return;
    }
    const std::uint8_t remaining = data.front();
    data.erase(data.begin());

    if (peer.unreliable && stream == *peer.unreliable) {
        if (remaining == 0) {
            push(LinkEvent{LinkEvent::Kind::Message, peer.link, std::move(data)});
        }
        return;
    }
    if (!peer.reliable || stream != *peer.reliable) {
        return;
    }
    if (peer.expectedRemaining >= 0 && remaining != peer.expectedRemaining - 1) {
        peer.dead = true;
        return;
    }
    peer.assembling.insert(peer.assembling.end(), data.begin(), data.end());
    if (peer.assembling.size() > kMaxMessageBytes) {
        peer.dead = true;
        return;
    }
    if (remaining > 0) {
        peer.expectedRemaining = remaining;
        return;
    }
    peer.expectedRemaining = -1;
    push(LinkEvent{LinkEvent::Kind::Message, peer.link, std::exchange(peer.assembling, {})});
}

void NetherNetServer::Impl::announce(Peer& peer) {
    peer.announced = true;
    const std::shared_ptr<Outbox> outbox = peer.outbox;
    const std::shared_ptr<io::EventLoop> shared = loop;
    peer.link = std::make_shared<NetherNetLink>(
        [outbox, shared](std::span<const std::uint8_t> payload) {
            {
                const std::lock_guard<std::mutex> guard{outbox->lock};
                if (outbox->closing) {
                    return;
                }
                outbox->items.emplace_back(payload.begin(), payload.end());
            }
            (void)shared->wake();
        },
        [outbox, shared] {
            {
                const std::lock_guard<std::mutex> guard{outbox->lock};
                outbox->closing = true;
            }
            (void)shared->wake();
        },
        peer.endpoint, peer.identityKey);
    connections.fetch_add(1);
    push(LinkEvent{LinkEvent::Kind::Connected, peer.link, {}});
}

void NetherNetServer::Impl::flushOutbox(Peer& peer, Clock::time_point now) {
    if (!peer.announced || !peer.reliable) {
        return;
    }
    std::deque<std::vector<std::uint8_t>> items;
    bool closing = false;
    {
        const std::lock_guard<std::mutex> guard{peer.outbox->lock};
        items.swap(peer.outbox->items);
        closing = peer.outbox->closing;
    }
    for (const auto& payload : items) {
        const std::size_t total = std::max<std::size_t>(1, (payload.size() + kSegmentBytes - 1) / kSegmentBytes);
        if (total > kMaxSegments) {
            peer.dead = true;
            return;
        }
        for (std::size_t index = 0; index < total; ++index) {
            const std::size_t from = index * kSegmentBytes;
            const std::size_t count = std::min(kSegmentBytes, payload.size() - from);
            std::vector<std::uint8_t> segment;
            segment.reserve(count + 1);
            segment.push_back(static_cast<std::uint8_t>(total - 1 - index));
            segment.insert(segment.end(), payload.begin() + static_cast<std::ptrdiff_t>(from),
                           payload.begin() + static_cast<std::ptrdiff_t>(from + count));
            peer.segments.push_back(std::move(segment));
        }
    }

    while (!peer.segments.empty()) {
        const nethernet::SctpSend result = peer.sctp->send(*peer.reliable, kPpidBinary, true, peer.segments.front());
        if (result == nethernet::SctpSend::WouldBlock) {
            break;
        }
        if (result == nethernet::SctpSend::Failed) {
            peer.dead = true;
            return;
        }
        peer.segments.pop_front();
    }

    std::vector<std::vector<std::uint8_t>> packets;
    peer.sctp->takeOutput(packets);
    for (const auto& packet : packets) {
        (void)peer.dtls->send(packet);
    }

    if (closing && peer.segments.empty() && !peer.closeAt) {
        peer.closeAt = now + kCloseGrace;
    }
    if (peer.closeAt && now >= *peer.closeAt) {
        peer.dead = true;
    }
}

void NetherNetServer::Impl::removePeer(Peer& peer) {
    if (peer.iceReady) {
        byEndpoint.erase(peer.endpoint);
    }
    byUfrag.erase(peer.ufrag);
    if (peer.dtls != nullptr) {
        peer.dtls->close();
    }
    if (peer.sctp != nullptr) {
        peer.sctp->shutdown();
    }
    if (!peer.announced && abandoned) {
        abandoned(peer.origin);
    }
    if (peer.announced) {
        peer.link->markClosed();
        connections.fetch_sub(1);
        push(LinkEvent{LinkEvent::Kind::Closed, peer.link, {}});
    }
}

NetherNetServer::NetherNetServer(NetherNetConfig config) : impl_(std::make_unique<Impl>()) {
    impl_->config = std::move(config);
}

NetherNetServer::~NetherNetServer() { stop(); }

void NetherNetServer::onStatus(StatusProvider provider) { impl_->status = std::move(provider); }

void NetherNetServer::onSignal(SignalGate gate) { impl_->signal = std::move(gate); }

void NetherNetServer::onAbandon(AbandonHandler handler) { impl_->abandoned = std::move(handler); }

void NetherNetServer::onConnect(LinkHandler handler) { impl_->connected = std::move(handler); }

void NetherNetServer::onMessage(MessageHandler handler) { impl_->message = std::move(handler); }

void NetherNetServer::onDisconnect(LinkHandler handler) { impl_->disconnected = std::move(handler); }

std::expected<void, std::string> NetherNetServer::start() {
    Impl& impl = *impl_;
    impl.credentials = std::make_unique<nethernet::DtlsCredentials>();
    if (!impl.credentials->ok()) {
        return std::unexpected{"could not make the DTLS certificate"};
    }

    auto listener = io::TcpListener::bind(impl.config.signalingPort);
    if (!listener) {
        return std::unexpected{"could not bind TCP port " + std::to_string(impl.config.signalingPort)};
    }
    impl.listener = std::move(*listener);
    if (!impl.listener.setNonBlocking()) {
        return std::unexpected{"could not make the signaling socket non-blocking"};
    }
    const auto signaling = impl.listener.localPort();
    impl.signalingPort = signaling ? *signaling : impl.config.signalingPort;

    const std::uint16_t wanted = impl.config.icePort != 0 ? impl.config.icePort : impl.signalingPort;
    auto udp = io::UdpSocket::bind(wanted);
    if (!udp) {
        impl.listener.close();
        return std::unexpected{"could not bind UDP port " + std::to_string(wanted)};
    }
    impl.udp = std::move(*udp);
    const auto ice = impl.udp.localPort();
    impl.icePort = ice ? *ice : wanted;

    if (!impl.loop->watch(impl.listener.nativeHandle(), io::IoInterest{true, false}, kListenerToken) ||
        !impl.loop->watch(impl.udp.nativeHandle(), io::IoInterest{true, false}, kUdpToken)) {
        return std::unexpected{"could not watch the sockets"};
    }

    impl.running.store(true);
    impl.thread = std::jthread{[&impl] { impl.run(); }};
    return {};
}

void NetherNetServer::stop() {
    Impl& impl = *impl_;
    if (!impl.running.exchange(false)) {
        return;
    }
    (void)impl.loop->wake();
    if (impl.thread.joinable()) {
        impl.thread.join();
    }
    impl.udp.close();
    impl.listener.close();
    drain();
}

void NetherNetServer::drain() {
    std::vector<LinkEvent> events;
    {
        const std::lock_guard<std::mutex> guard{impl_->eventLock};
        events.swap(impl_->events);
    }
    for (LinkEvent& event : events) {
        switch (event.kind) {
        case LinkEvent::Kind::Connected:
            if (impl_->connected) {
                impl_->connected(event.link);
            }
            break;
        case LinkEvent::Kind::Message:
            if (impl_->message && !event.link->closed()) {
                impl_->message(event.link, event.data);
            }
            break;
        case LinkEvent::Kind::Closed:
            if (impl_->disconnected) {
                impl_->disconnected(event.link);
            }
            break;
        }
    }
}

std::uint16_t NetherNetServer::signalingPort() const noexcept { return impl_->signalingPort; }

std::uint16_t NetherNetServer::icePort() const noexcept { return impl_->icePort; }

std::size_t NetherNetServer::connectionCount() const noexcept { return impl_->connections.load(); }

} // namespace basalt::wire::bedrock
