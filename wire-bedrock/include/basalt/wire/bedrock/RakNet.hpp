#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/io/UdpSocket.hpp>

#include <array>
#include <cstdint>
#include <span>

namespace basalt::wire::bedrock {

/// RakNet's own message ids. Everything below 0x80 belongs to the transport; 0xfe is the game, and
/// it is the only one this layer hands upwards.
enum class RakId : std::uint8_t {
    ConnectedPing = 0x00,
    UnconnectedPing = 0x01,
    UnconnectedPingOpenConnections = 0x02,
    ConnectedPong = 0x03,
    OpenConnectionRequest1 = 0x05,
    OpenConnectionReply1 = 0x06,
    OpenConnectionRequest2 = 0x07,
    OpenConnectionReply2 = 0x08,
    ConnectionRequest = 0x09,
    ConnectionRequestAccepted = 0x10,
    ConnectionRequestFailed = 0x11,
    AlreadyConnected = 0x12,
    NewIncomingConnection = 0x13,
    NoFreeIncomingConnections = 0x14,
    DisconnectionNotification = 0x15,
    ConnectionLost = 0x16,
    ConnectionBanned = 0x17,
    IncompatibleProtocolVersion = 0x19,
    UnconnectedPong = 0x1c,
    Game = 0xfe,
};

/// How a message is delivered. The number is the wire value: it travels in the top three bits of a
/// frame's flags, so the order of this enum is the protocol's and not ours to sort.
enum class Reliability : std::uint8_t {
    Unreliable = 0,
    UnreliableSequenced = 1,
    Reliable = 2,
    ReliableOrdered = 3,
    ReliableSequenced = 4,
    UnreliableWithAckReceipt = 5,
    UnreliableSequencedWithAckReceipt = 6,
    ReliableWithAckReceipt = 7,
};

[[nodiscard]] constexpr bool isReliable(Reliability reliability) noexcept {
    return reliability == Reliability::Reliable || reliability == Reliability::ReliableOrdered ||
           reliability == Reliability::ReliableSequenced || reliability == Reliability::ReliableWithAckReceipt;
}

[[nodiscard]] constexpr bool isOrdered(Reliability reliability) noexcept {
    return reliability == Reliability::ReliableOrdered;
}

[[nodiscard]] constexpr bool isSequenced(Reliability reliability) noexcept {
    return reliability == Reliability::UnreliableSequenced || reliability == Reliability::ReliableSequenced ||
           reliability == Reliability::UnreliableSequencedWithAckReceipt;
}

/// The sixteen bytes every unconnected message carries, which is how a RakNet datagram is told from
/// whatever else arrives at a UDP port.
inline constexpr std::array<std::uint8_t, 16> kMagic{0x00, 0xff, 0xff, 0x00, 0xfe, 0xfe, 0xfe, 0xfe,
                                                     0xfd, 0xfd, 0xfd, 0xfd, 0x12, 0x34, 0x56, 0x78};

/// The RakNet protocol version Mojang's client speaks. Anything else gets told so by number.
inline constexpr std::uint8_t kRakNetProtocol = 11;

inline constexpr std::uint16_t kMinimumMtu = 576;
inline constexpr std::uint16_t kMaximumMtu = 1400;

/// What an IPv4 datagram spends before RakNet gets a byte: 20 for IP, 8 for UDP.
inline constexpr std::size_t kIpv4Overhead = 28;

inline constexpr std::size_t kDatagramHeaderSize = 4;

/// Flags of a frame with everything set: three bytes of base, ten of reliability and ordering, ten
/// of split. A frame never needs more room than this for its header.
inline constexpr std::size_t kMaxFrameHeaderSize = 23;

/// Flags of the first byte of a datagram.
inline constexpr std::uint8_t kFlagValid = 0x80;
inline constexpr std::uint8_t kFlagAck = 0x40;
inline constexpr std::uint8_t kFlagNak = 0x20;
inline constexpr std::uint8_t kFlagSplit = 0x10;

/// A session with nothing coming in for this long is gone, whether it said so or not.
inline constexpr std::uint64_t kSessionTimeoutMs = 10000;

/// Three bytes, little endian, which is how RakNet writes every one of its counters. The rest of
/// the protocol is big endian, and no, there is no reason for it.
void writeMediumLE(io::ByteBuffer& buffer, std::uint32_t value);

[[nodiscard]] std::uint32_t readMediumLE(io::ByteBuffer& buffer) noexcept;

/// An address as RakNet writes it: a version byte, then the four octets with every bit flipped,
/// then the port. The flipping is RakNet's, not a mistake.
void writeAddress(io::ByteBuffer& buffer, const io::UdpEndpoint& endpoint);

[[nodiscard]] io::UdpEndpoint readAddress(io::ByteBuffer& buffer) noexcept;

[[nodiscard]] bool hasMagic(std::span<const std::uint8_t> bytes) noexcept;

} // namespace basalt::wire::bedrock
