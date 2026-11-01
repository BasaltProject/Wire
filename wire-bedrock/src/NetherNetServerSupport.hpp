#pragma once

#include "NetherNetDtls.hpp"
#include "NetherNetHttp.hpp"
#include "NetherNetSctp.hpp"

#include <basalt/wire/bedrock/NetherNetIdentity.hpp>
#include <basalt/wire/bedrock/NetherNetSdp.hpp>
#include <basalt/wire/bedrock/NetherNetServer.hpp>
#include <basalt/wire/bedrock/NetherNetStun.hpp>
#include <basalt/wire/io/EventLoop.hpp>
#include <basalt/wire/io/TcpSocket.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <random>
#include <thread>
#include <unordered_map>
#include <utility>

namespace basalt::wire::bedrock {
namespace impl {

using Clock = std::chrono::steady_clock;

constexpr std::uint64_t kListenerToken = 1;
constexpr std::uint64_t kUdpToken = 2;
constexpr std::uint64_t kFirstHttpToken = 100;

constexpr std::uint32_t kPpidBinary = 53;
constexpr std::size_t kSegmentBytes = 10'000;
constexpr std::size_t kMaxSegments = 255;
constexpr std::size_t kMaxMessageBytes = 16U << 20U;
constexpr std::string_view kReliableLabel = "ReliableDataChannel";
constexpr std::string_view kUnreliableLabel = "UnreliableDataChannel";

constexpr auto kNegotiationTimeout = std::chrono::seconds{10};
constexpr auto kSilenceTimeout = std::chrono::seconds{30};
constexpr auto kHttpTimeout = std::chrono::seconds{10};
constexpr std::size_t kMaxHttpConnections = 1024;
constexpr std::uint32_t kMaxHttpPerAddress = 16;
constexpr auto kCloseGrace = std::chrono::milliseconds{300};

inline std::string randomText(std::size_t length) {
    static constexpr std::string_view kAlphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device device;
    std::string out;
    for (std::size_t i = 0; i < length; ++i) {
        out += kAlphabet[device() % kAlphabet.size()];
    }
    return out;
}

inline std::string jsonEscaped(std::string_view text) {
    std::string out;
    for (const char c : text) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out += ' ';
            } else {
                out += c;
            }
        }
    }
    return out;
}

inline std::string statusJson(const NetherNetStatus& status) {
    return R"({"name":")" + jsonEscaped(status.name) + R"(","protocol":)" + std::to_string(status.protocol) +
           R"(,"version":")" + jsonEscaped(status.version) + R"(","level":")" + jsonEscaped(status.level) +
           R"(","players":)" + std::to_string(status.players) + R"(,"maxPlayers":)" +
           std::to_string(status.maxPlayers) + R"(,"gameType":)" + std::to_string(status.gameType) + "}";
}

struct Outbox {
    std::mutex lock;
    std::deque<std::vector<std::uint8_t>> items;
    bool closing = false;
};

struct Peer {
    std::uint64_t id = 0;
    std::string ufrag;
    std::string pwd;
    std::string origin;
    std::vector<std::uint8_t> identityKey;
    std::unique_ptr<nethernet::DtlsSession> dtls;
    std::unique_ptr<nethernet::SctpEngine> sctp;
    io::UdpEndpoint endpoint;
    bool iceReady = false;
    bool sctpUp = false;
    bool announced = false;
    bool dead = false;
    Clock::time_point created;
    Clock::time_point lastHeard;
    std::optional<std::uint16_t> reliable;
    std::optional<std::uint16_t> unreliable;
    std::shared_ptr<NetherNetLink> link;
    std::shared_ptr<Outbox> outbox = std::make_shared<Outbox>();
    std::deque<std::vector<std::uint8_t>> segments;
    std::vector<std::uint8_t> assembling;
    int expectedRemaining = -1;
    std::optional<Clock::time_point> closeAt;
};

struct HttpConnection {
    io::TcpConnection socket;
    std::string input;
    std::string output;
    Clock::time_point deadline;
    bool responding = false;
};

struct LinkEvent {
    enum class Kind : std::uint8_t { Connected, Message, Closed };

    Kind kind = Kind::Message;
    std::shared_ptr<NetherNetLink> link;
    std::vector<std::uint8_t> data;
};

} // namespace impl

using namespace impl;

} // namespace basalt::wire::bedrock
