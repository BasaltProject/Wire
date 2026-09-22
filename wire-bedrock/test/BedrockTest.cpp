#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/bedrock/BedrockBatch.hpp>
#include <basalt/wire/bedrock/BedrockBlocks.hpp>
#include <basalt/wire/bedrock/BedrockCipher.hpp>
#include <basalt/wire/bedrock/BedrockConnection.hpp>
#include <basalt/wire/bedrock/BedrockGate.hpp>
#include <basalt/wire/bedrock/BedrockItems.hpp>
#include <basalt/wire/bedrock/BedrockKeys.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>
#include <basalt/wire/bedrock/RakFrame.hpp>
#include <basalt/wire/bedrock/RakNet.hpp>
#include <basalt/wire/bedrock/RakServer.hpp>
#include <basalt/wire/bedrock/RakSession.hpp>
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace basalt::wire;
using namespace basalt::wire::bedrock;

namespace {

std::vector<std::uint8_t> payload(std::size_t size, std::uint8_t seed) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t index = 0; index < size; ++index) {
        bytes[index] = static_cast<std::uint8_t>(seed + index);
    }
    return bytes;
}

io::UdpEndpoint anyPeer() { return io::UdpEndpoint{0x0100007fU, 19132}; }

/// A pair of sessions that hand each other their datagrams, so a test can watch a message travel
/// the whole reliability layer instead of only its encoding.
struct Link {
    RakSession left{anyPeer(), kMaximumMtu, 1, 0};
    RakSession right{io::UdpEndpoint{0x0100007fU, 19133}, kMaximumMtu, 2, 0};
    std::vector<std::vector<std::uint8_t>> leftInbox;
    std::vector<std::vector<std::uint8_t>> rightInbox;
    std::vector<std::vector<std::uint8_t>> wire;

    void collect(RakSession& from, std::uint64_t nowMs) {
        from.flush(nowMs,
                   [this](std::span<const std::uint8_t> bytes) { wire.emplace_back(bytes.begin(), bytes.end()); });
    }

    void deliver(RakSession& to, std::vector<std::vector<std::uint8_t>>& inbox, std::uint64_t nowMs) {
        for (const std::vector<std::uint8_t>& datagram : wire) {
            to.receiveDatagram(datagram, nowMs, inbox);
        }
        wire.clear();
    }
};

} // namespace

TEST_CASE("a frame survives its own encoding at every reliability") {
    for (std::uint8_t id = 0; id <= 7; ++id) {
        RakFrame frame;
        frame.reliability = static_cast<Reliability>(id);
        frame.reliableIndex = 0x123456;
        frame.sequenceIndex = 0x654321;
        frame.orderIndex = 0x0f0f0f;
        frame.orderChannel = 3;
        frame.body = payload(37, id);

        io::ByteBuffer buffer;
        frame.encode(buffer);
        CHECK(buffer.writerIndex() == frame.encodedSize());

        RakFrame decoded;
        REQUIRE(RakFrame::decode(buffer, decoded));
        CHECK(decoded.reliability == frame.reliability);
        CHECK(decoded.body == frame.body);
        if (isReliable(frame.reliability)) {
            CHECK(decoded.reliableIndex == frame.reliableIndex);
        }
        if (isSequenced(frame.reliability)) {
            CHECK(decoded.sequenceIndex == frame.sequenceIndex);
        }
        if (isOrdered(frame.reliability) || isSequenced(frame.reliability)) {
            CHECK(decoded.orderIndex == frame.orderIndex);
            CHECK(decoded.orderChannel == frame.orderChannel);
        }
    }
}

TEST_CASE("a split frame keeps its part numbers") {
    RakFrame frame;
    frame.reliability = Reliability::ReliableOrdered;
    frame.split = true;
    frame.partCount = 9;
    frame.partId = 0x4321;
    frame.partIndex = 7;
    frame.body = payload(11, 1);

    io::ByteBuffer buffer;
    frame.encode(buffer);
    RakFrame decoded;
    REQUIRE(RakFrame::decode(buffer, decoded));
    CHECK(decoded.split);
    CHECK(decoded.partCount == 9);
    CHECK(decoded.partId == 0x4321);
    CHECK(decoded.partIndex == 7);
    CHECK(decoded.body == frame.body);
}

TEST_CASE("acknowledgement ranges collapse and come back the same") {
    const std::vector<std::uint32_t> sequences{1, 2, 3, 7, 9, 10};
    std::vector<SequenceRange> ranges;
    collapseRanges(sequences, ranges);
    REQUIRE(ranges.size() == 3);
    CHECK(ranges[0].start == 1);
    CHECK(ranges[0].end == 3);
    CHECK(ranges[1].start == 7);
    CHECK(ranges[1].end == 7);

    io::ByteBuffer buffer;
    writeRanges(buffer, ranges);
    std::vector<SequenceRange> back;
    REQUIRE(readRanges(buffer, back));
    REQUIRE(back.size() == ranges.size());
    for (std::size_t index = 0; index < ranges.size(); ++index) {
        CHECK(back[index].start == ranges[index].start);
        CHECK(back[index].end == ranges[index].end);
    }
}

TEST_CASE("an address is written with its octets flipped, the way RakNet does") {
    io::ByteBuffer buffer;
    const io::UdpEndpoint endpoint{0x0100007fU, 19132};
    writeAddress(buffer, endpoint);
    CHECK(buffer.writerIndex() == 7);
    CHECK(buffer.peekByte(0) == 4);
    CHECK(buffer.peekByte(1) == static_cast<std::uint8_t>(~127));

    const io::UdpEndpoint back = readAddress(buffer);
    CHECK(back.address == endpoint.address);
    CHECK(back.port == endpoint.port);
}

TEST_CASE("a message larger than the mtu arrives whole") {
    Link link;
    const std::vector<std::uint8_t> message = payload(20000, 5);
    link.right.sendGame(message);
    link.collect(link.right, 0);
    CHECK(link.wire.size() > 10);
    link.deliver(link.left, link.leftInbox, 0);

    REQUIRE(link.leftInbox.size() == 1);
    CHECK(link.leftInbox[0] == message);
}

TEST_CASE("ordered messages that arrive out of order are handed over in order") {
    Link link;
    for (std::uint8_t index = 0; index < 4; ++index) {
        link.right.sendGame(payload(10, index));
        link.collect(link.right, 0);
    }
    REQUIRE(link.wire.size() == 4);
    std::swap(link.wire[0], link.wire[3]);
    std::swap(link.wire[1], link.wire[2]);
    link.deliver(link.left, link.leftInbox, 0);

    REQUIRE(link.leftInbox.size() == 4);
    for (std::uint8_t index = 0; index < 4; ++index) {
        CHECK(link.leftInbox[index] == payload(10, index));
    }
}

TEST_CASE("a duplicated datagram is delivered once") {
    Link link;
    link.right.sendGame(payload(16, 9));
    link.collect(link.right, 0);
    REQUIRE(link.wire.size() == 1);
    link.wire.push_back(link.wire[0]);
    link.deliver(link.left, link.leftInbox, 0);
    CHECK(link.leftInbox.size() == 1);
}

TEST_CASE("a reliable message left unacknowledged goes out again") {
    Link link;
    link.right.sendGame(payload(16, 3));
    link.collect(link.right, 0);
    REQUIRE(link.wire.size() == 1);
    link.wire.clear();

    link.collect(link.right, kResendTimeoutMs);
    REQUIRE(link.wire.size() == 1);

    link.deliver(link.left, link.leftInbox, kResendTimeoutMs);
    CHECK(link.leftInbox.size() == 1);

    // The acknowledgement the receiver now owes stops the third attempt.
    link.collect(link.left, kResendTimeoutMs);
    link.deliver(link.right, link.rightInbox, kResendTimeoutMs);
    link.collect(link.right, kResendTimeoutMs * 4);
    CHECK(link.wire.empty());
}

TEST_CASE("the raknet handshake answers a ping and opens a connection") {
    RakServerConfig config;
    config.port = 0;
    RakServer server{config};
    REQUIRE(server.start().has_value());
    server.onAdvertise([] { return std::string{"MCPE;Basalt;924;26.40;0;10;1;Basalt;Survival;1;19132;19133;"}; });

    auto client = io::UdpSocket::bind(0);
    REQUIRE(client.has_value());
    const io::UdpEndpoint serverEndpoint{0x0100007fU, server.port()};

    io::ByteBuffer ping;
    ping.writeByte(static_cast<std::uint8_t>(RakId::UnconnectedPing));
    ping.writeLong(12345);
    ping.writeBytes(kMagic);
    ping.writeLong(77);
    REQUIRE(client->sendTo(ping.readable(), serverEndpoint).has_value());

    server.poll(0, 200);

    std::vector<std::uint8_t> answer(2048);
    io::UdpEndpoint from{};
    auto received = client->receiveFrom(answer, from, 500);
    REQUIRE(received.has_value());
    REQUIRE(received->has_value());

    io::ByteBuffer pong =
        io::ByteBuffer::wrap({answer.begin(), answer.begin() + static_cast<std::ptrdiff_t>(**received)});
    CHECK(pong.readByte() == static_cast<std::uint8_t>(RakId::UnconnectedPong));
    CHECK(pong.readLong() == 12345);
    CHECK(pong.readLong() == static_cast<std::int64_t>(server.guid()));
    CHECK(hasMagic(pong.readable()));

    // Open connection, both halves, and the session exists afterwards.
    io::ByteBuffer request1;
    request1.writeByte(static_cast<std::uint8_t>(RakId::OpenConnectionRequest1));
    request1.writeBytes(kMagic);
    request1.writeByte(kRakNetProtocol);
    const std::vector<std::uint8_t> padding(400, 0);
    request1.writeBytes(padding);
    REQUIRE(client->sendTo(request1.readable(), serverEndpoint).has_value());
    server.poll(0, 200);

    received = client->receiveFrom(answer, from, 500);
    REQUIRE(received.has_value());
    REQUIRE(received->has_value());
    CHECK(answer[0] == static_cast<std::uint8_t>(RakId::OpenConnectionReply1));

    io::ByteBuffer request2;
    request2.writeByte(static_cast<std::uint8_t>(RakId::OpenConnectionRequest2));
    request2.writeBytes(kMagic);
    writeAddress(request2, serverEndpoint);
    request2.writeShort(static_cast<std::int16_t>(1400));
    request2.writeLong(4242);
    REQUIRE(client->sendTo(request2.readable(), serverEndpoint).has_value());
    server.poll(0, 200);

    received = client->receiveFrom(answer, from, 500);
    REQUIRE(received.has_value());
    REQUIRE(received->has_value());
    CHECK(answer[0] == static_cast<std::uint8_t>(RakId::OpenConnectionReply2));
    CHECK(server.sessionCount() == 1);
}

TEST_CASE("varints round-trip, and a truncated one fails instead of guessing") {
    io::ByteBuffer buffer;
    const std::vector<std::int32_t> values{0, 1, -1, 127, -128, 2147483647, -2147483648};
    for (const std::int32_t value : values) {
        writeVarInt(buffer, value);
    }
    for (const std::int32_t value : values) {
        CHECK(readVarInt(buffer) == value);
    }
    CHECK_FALSE(buffer.failed());

    io::ByteBuffer broken;
    for (int index = 0; index < 6; ++index) {
        broken.writeByte(0x80);
    }
    static_cast<void>(readVarUInt(broken));
    CHECK(broken.failed());
}

TEST_CASE("the little-endian primitives are the opposite of the java ones") {
    io::ByteBuffer buffer;
    writeIntLE(buffer, 1);
    CHECK(buffer.peekByte(0) == 1);
    CHECK(buffer.peekByte(3) == 0);
    CHECK(readIntLE(buffer) == 1);

    writeString(buffer, "Basalt");
    CHECK(readString(buffer) == "Basalt");

    writeFloatLE(buffer, 1.5F);
    CHECK(readFloatLE(buffer) == 1.5F);
}

TEST_CASE("a batch comes back as the packets that went in") {
    const std::vector<std::vector<std::uint8_t>> packets{payload(4, 1), payload(600, 2), payload(9, 3)};

    SUBCASE("uncompressed, before the network settings are agreed") {
        BatchSettings settings;
        io::ByteBuffer out;
        REQUIRE(packBatch(packets, settings, out));
        std::vector<std::vector<std::uint8_t>> back;
        REQUIRE(unpackBatch(out.readable(), settings, back));
        CHECK(back == packets);
    }

    SUBCASE("compressed, with the algorithm byte in front") {
        BatchSettings settings;
        settings.enabled = true;
        io::ByteBuffer out;
        REQUIRE(packBatch(packets, settings, out));
        CHECK(out.peekByte(0) == static_cast<std::uint8_t>(Compression::Zlib));
        CHECK(out.readableBytes() < 613);
        std::vector<std::vector<std::uint8_t>> back;
        REQUIRE(unpackBatch(out.readable(), settings, back));
        CHECK(back == packets);
    }

    SUBCASE("a batch under the threshold says so instead of compressing") {
        BatchSettings settings;
        settings.enabled = true;
        const std::vector<std::vector<std::uint8_t>> small{payload(8, 7)};
        io::ByteBuffer out;
        REQUIRE(packBatch(small, settings, out));
        CHECK(out.peekByte(0) == static_cast<std::uint8_t>(Compression::None));
        std::vector<std::vector<std::uint8_t>> back;
        REQUIRE(unpackBatch(out.readable(), settings, back));
        CHECK(back == small);
    }
}

TEST_CASE("the packet cipher is aes-256 in counter mode, checked against a known keystream") {
    SessionKey key{};
    for (std::size_t index = 0; index < key.size(); ++index) {
        key[index] = static_cast<std::uint8_t>(index);
    }

    const std::string text = "basalt bedrock";
    const std::span<const std::uint8_t> plain{reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};

    BedrockCipher sender{key};
    std::vector<std::uint8_t> encrypted;
    sender.encrypt(plain, encrypted);

    const std::vector<std::uint8_t> expected{0x25, 0x63, 0xa5, 0x7a, 0xa9, 0x91, 0xe2,
                                             0x79, 0xe8, 0x25, 0xe5, 0xe4, 0xd2, 0x82};
    REQUIRE(encrypted.size() == text.size() + 8);
    CHECK(std::vector<std::uint8_t>(encrypted.begin(), encrypted.begin() + 14) == expected);

    BedrockCipher receiver{key};
    std::vector<std::uint8_t> decrypted;
    REQUIRE(receiver.decrypt(encrypted, decrypted));
    CHECK(std::string(decrypted.begin(), decrypted.end()) == text);

    // A second packet moves both the counter and the keystream, so the same text encrypts
    // differently and still arrives.
    std::vector<std::uint8_t> second;
    sender.encrypt(plain, second);
    CHECK(second != encrypted);
    REQUIRE(receiver.decrypt(second, decrypted));
    CHECK(std::string(decrypted.begin(), decrypted.end()) == text);

    // A flipped bit is caught by the checksum rather than handed over as a packet.
    second[3] ^= 0x01;
    BedrockCipher third{key};
    CHECK_FALSE(third.decrypt(second, decrypted));
}

TEST_CASE("the session key is the digest of the salt and the shared secret") {
    const std::string salt = "salt";
    const std::string secret = "secret";
    const SessionKey key = deriveSessionKey({reinterpret_cast<const std::uint8_t*>(salt.data()), salt.size()},
                                            {reinterpret_cast<const std::uint8_t*>(secret.data()), secret.size()});

    const std::vector<std::uint8_t> expected{0xbe, 0xde, 0x90, 0x38, 0x6d, 0x45, 0x0c, 0xea, 0x8b, 0x77, 0xb8,
                                             0x22, 0xf8, 0x88, 0x70, 0x65, 0xe4, 0xe5, 0xab, 0xf1, 0x32, 0xc2,
                                             0xf9, 0xdc, 0xcf, 0xcc, 0x7f, 0xbd, 0x4c, 0xba, 0x5e, 0x35};
    CHECK(std::vector<std::uint8_t>(key.begin(), key.end()) == expected);
}

TEST_CASE("a key pair signs a token its own public half verifies") {
    const auto keys = EcKeyPair::generate();
    REQUIRE(keys.has_value());
    CHECK_FALSE(keys->publicKeyDer().empty());

    const auto token = keys->signJwt(R"({"salt":"c2FsdA"})");
    REQUIRE(token.has_value());

    const auto parts = decodeJwt(*token);
    REQUIRE(parts.has_value());
    CHECK(jsonString(parts->header, "alg") == "ES384");
    CHECK(jsonString(parts->header, "x5u") == keys->publicKeyBase64());
    CHECK(jsonString(parts->payload, "salt") == "c2FsdA");
    CHECK(verifyJwt(*parts, keys->publicKeyDer()));

    // The other key pair did not sign it, and a changed payload is not the one that was signed.
    const auto other = EcKeyPair::generate();
    REQUIRE(other.has_value());
    CHECK_FALSE(verifyJwt(*parts, other->publicKeyDer()));

    JwtParts tampered = *parts;
    tampered.signingInput += "x";
    CHECK_FALSE(verifyJwt(tampered, keys->publicKeyDer()));
}

TEST_CASE("both ends of a key exchange arrive at the same session key") {
    const auto server = EcKeyPair::generate();
    const auto client = EcKeyPair::generate();
    REQUIRE(server.has_value());
    REQUIRE(client.has_value());

    const auto serverSide = server->sharedSecret(client->publicKeyDer());
    const auto clientSide = client->sharedSecret(server->publicKeyDer());
    REQUIRE(serverSide.has_value());
    REQUIRE(clientSide.has_value());
    CHECK(*serverSide == *clientSide);
    CHECK(serverSide->size() == 48);

    const std::vector<std::uint8_t> salt{1, 2, 3, 4};
    CHECK(deriveSessionKey(salt, *serverSide) == deriveSessionKey(salt, *clientSide));
}

namespace {

/// A client made of the same pieces the server uses, so a test can walk the login end to end:
/// batches, compression, the key exchange and the cipher, over a real pair of RakNet sessions.
struct LoginHarness {
    RakSession serverTransport{io::UdpEndpoint{0x0100007fU, 19132}, kMaximumMtu, 1, 0};
    RakSession clientTransport{io::UdpEndpoint{0x0100007fU, 19133}, kMaximumMtu, 2, 0};
    std::shared_ptr<EcKeyPair> serverKeys = std::make_shared<EcKeyPair>(*EcKeyPair::generate());
    EcKeyPair clientKeys = *EcKeyPair::generate();
    BedrockConnection connection{serverTransport, serverKeys, "1.26.40"};

    BatchSettings clientBatch;
    std::unique_ptr<BedrockCipher> clientCipher;
    std::vector<std::pair<BedrockPacket, std::vector<std::uint8_t>>> received;

    void sendToServer(BedrockPacket packet, std::span<const std::uint8_t> body) {
        io::ByteBuffer encoded;
        writeVarUInt(encoded, static_cast<std::uint32_t>(packet));
        encoded.writeBytes(body);
        const std::vector<std::vector<std::uint8_t>> packets{{encoded.readable().begin(), encoded.readable().end()}};

        io::ByteBuffer batch;
        REQUIRE(packBatch(packets, clientBatch, batch));

        std::vector<std::uint8_t> payload{batch.readable().begin(), batch.readable().end()};
        if (clientCipher) {
            std::vector<std::uint8_t> encrypted;
            clientCipher->encrypt(payload, encrypted);
            payload = encrypted;
        }
        clientTransport.sendGame(payload);

        std::vector<std::vector<std::uint8_t>> datagrams;
        clientTransport.flush(0, [&datagrams](std::span<const std::uint8_t> bytes) {
            datagrams.emplace_back(bytes.begin(), bytes.end());
        });
        std::vector<std::vector<std::uint8_t>> messages;
        for (const std::vector<std::uint8_t>& datagram : datagrams) {
            serverTransport.receiveDatagram(datagram, 0, messages);
        }
        for (const std::vector<std::uint8_t>& message : messages) {
            connection.receive(message);
        }
        pump();
    }

    /// Moves whatever the server queued back to the client and decodes it.
    void pump() {
        std::vector<std::vector<std::uint8_t>> datagrams;
        serverTransport.flush(0, [&datagrams](std::span<const std::uint8_t> bytes) {
            datagrams.emplace_back(bytes.begin(), bytes.end());
        });
        std::vector<std::vector<std::uint8_t>> messages;
        for (const std::vector<std::uint8_t>& datagram : datagrams) {
            clientTransport.receiveDatagram(datagram, 0, messages);
        }

        for (const std::vector<std::uint8_t>& message : messages) {
            std::span<const std::uint8_t> batch = message;
            std::vector<std::uint8_t> plain;
            if (clientCipher) {
                REQUIRE(clientCipher->decrypt(message, plain));
                batch = plain;
            }
            std::vector<std::vector<std::uint8_t>> packets;
            REQUIRE(unpackBatch(batch, clientBatch, packets));
            for (const std::vector<std::uint8_t>& packet : packets) {
                io::ByteBuffer reader = io::ByteBuffer::wrap({packet.begin(), packet.end()});
                const auto id = static_cast<BedrockPacket>(readVarUInt(reader) & 0x3ffU);
                const std::span<const std::uint8_t> body = reader.readable();
                received.emplace_back(id, std::vector<std::uint8_t>{body.begin(), body.end()});
            }
        }
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> take(BedrockPacket packet) {
        for (auto entry = received.begin(); entry != received.end(); ++entry) {
            if (entry->first == packet) {
                std::vector<std::uint8_t> body = entry->second;
                received.erase(entry);
                return body;
            }
        }
        return std::nullopt;
    }

    /// The login a client of this version sends: a chain of one entry it signed itself, which is
    /// what an offline client does.
    [[nodiscard]] std::vector<std::uint8_t> loginBody() {
        const std::string identity =
            R"({"identityPublicKey":")" + clientKeys.publicKeyBase64() +
            R"(","extraData":{"displayName":"Steve","identity":"00000000-0000-0000-0000-000000000001","XUID":"2535000000000000"}})";
        const std::string chainJwt = *clientKeys.signJwt(identity);
        const std::string certificate = R"({"chain":[")" + chainJwt + R"("]})";

        io::ByteBuffer auth;
        common::JsonObject object;
        object.set("AuthenticationType", common::JsonValue::of(static_cast<std::int64_t>(1)));
        object.set("Certificate", common::JsonValue::of(certificate));
        object.set("Token", common::JsonValue::of(std::string{}));
        const std::string authJson = common::writeJson(common::JsonValue::ofObject(object));
        const std::string clientJwt = *clientKeys.signJwt(R"({"DeviceOS":1})");

        io::ByteBuffer body;
        body.writeInt(kNativeBedrockProtocol);
        const std::uint32_t blobSize = static_cast<std::uint32_t>(authJson.size() + clientJwt.size() + 8);
        writeVarUInt(body, blobSize);
        writeIntLE(body, static_cast<std::int32_t>(authJson.size()));
        body.writeBytes({reinterpret_cast<const std::uint8_t*>(authJson.data()), authJson.size()});
        writeIntLE(body, static_cast<std::int32_t>(clientJwt.size()));
        body.writeBytes({reinterpret_cast<const std::uint8_t*>(clientJwt.data()), clientJwt.size()});
        return {body.readable().begin(), body.readable().end()};
    }

    /// What the client does with the handshake: take the salt and the server's key out of it and
    /// arrive at the same cipher from the other side.
    void acceptHandshake(std::span<const std::uint8_t> body) {
        io::ByteBuffer reader = io::ByteBuffer::wrap({body.begin(), body.end()});
        const std::string token = readString(reader);
        const auto parts = decodeJwt(token);
        REQUIRE(parts.has_value());

        const auto x5u = jsonString(parts->header, "x5u");
        REQUIRE(x5u.has_value());
        const auto serverDer = common::base64::decode(*x5u);
        REQUIRE(serverDer.has_value());
        CHECK(verifyJwt(*parts, *serverDer));

        const auto saltText = jsonString(parts->payload, "salt");
        REQUIRE(saltText.has_value());
        const auto salt = common::base64::decode(*saltText);
        REQUIRE(salt.has_value());

        const auto secret = clientKeys.sharedSecret(*serverDer);
        REQUIRE(secret.has_value());
        clientCipher = std::make_unique<BedrockCipher>(deriveSessionKey(*salt, *secret));
    }
};

} // namespace

TEST_CASE("a bedrock client gets from its first packet to the world") {
    LoginHarness harness;
    bool ready = false;
    std::string loggedInAs;
    harness.connection.onReady([&ready] { ready = true; });
    harness.connection.onLogin([&loggedInAs](const LoginRequest& login) { loggedInAs = login.name; });

    io::ByteBuffer request;
    request.writeInt(kNativeBedrockProtocol);
    harness.sendToServer(BedrockPacket::RequestNetworkSettings, request.readable());

    const auto settings = harness.take(BedrockPacket::NetworkSettings);
    REQUIRE(settings.has_value());
    harness.clientBatch.enabled = true;

    harness.sendToServer(BedrockPacket::Login, harness.loginBody());
    CHECK(loggedInAs == "Steve");
    CHECK(harness.connection.login().xuid == "2535000000000000");
    CHECK(harness.connection.protocolVersion() == kNativeBedrockProtocol);

    const auto handshake = harness.take(BedrockPacket::ServerToClientHandshake);
    REQUIRE(handshake.has_value());
    harness.acceptHandshake(*handshake);

    // From here on every batch is encrypted, in both directions.
    harness.sendToServer(BedrockPacket::ClientToServerHandshake, {});
    REQUIRE(harness.take(BedrockPacket::PlayStatus).has_value());
    REQUIRE(harness.take(BedrockPacket::ResourcePacksInfo).has_value());

    io::ByteBuffer response;
    writeVarUInt(response, 2); // Have all packs.
    writeString(response, "downloadingfinished");
    harness.sendToServer(BedrockPacket::ResourcePackClientResponse, response.readable());
    REQUIRE(harness.take(BedrockPacket::ResourcePackStack).has_value());

    io::ByteBuffer completed;
    writeVarUInt(completed, 3);
    writeString(completed, "resourcepackstackfinished");
    harness.sendToServer(BedrockPacket::ResourcePackClientResponse, completed.readable());

    CHECK(ready);
    CHECK(harness.connection.state() == BedrockState::Playing);

    // And the world packets a server sends next survive the same trip.
    StartGameInfo info;
    info.vanillaVersion = "1.26.40";
    io::ByteBuffer start;
    writeStartGame(start, info, kNativeBedrockProtocol);
    harness.connection.sendPacket(BedrockPacket::StartGame, start.readable());
    io::ByteBuffer chunk;
    writeEmptyLevelChunk(chunk, 0, 0, 24, 0, kNativeBedrockProtocol);
    harness.connection.sendPacket(BedrockPacket::LevelChunk, chunk.readable());
    harness.connection.flush();
    harness.pump();

    REQUIRE(harness.take(BedrockPacket::StartGame).has_value());
    const auto chunkBody = harness.take(BedrockPacket::LevelChunk);
    REQUIRE(chunkBody.has_value());
    io::ByteBuffer reader = io::ByteBuffer::wrap({chunkBody->begin(), chunkBody->end()});
    CHECK(readVarInt(reader) == 0);
    CHECK(readVarInt(reader) == 0);
    CHECK(readVarInt(reader) == 0);
    CHECK(readVarUInt(reader) == 0);
}

TEST_CASE("a line of chat is written the way a client reads one, both shapes") {
    SUBCASE("with an author") {
        io::ByteBuffer buffer;
        writeText(buffer, TextType::Chat, "Steve", "hello");
        const auto back = readText(buffer);
        REQUIRE(back.has_value());
        CHECK(back->type == TextType::Chat);
        CHECK(back->source == "Steve");
        CHECK(back->message == "hello");
    }

    SUBCASE("without one") {
        io::ByteBuffer buffer;
        writeText(buffer, TextType::Raw, "ignored", "Steve joined the game");
        const auto back = readText(buffer);
        REQUIRE(back.has_value());
        CHECK(back->type == TextType::Raw);
        CHECK(back->source.empty());
        CHECK(back->message == "Steve joined the game");
    }

    SUBCASE("an empty line becomes a space, because a client draws nothing for an empty one") {
        io::ByteBuffer buffer;
        writeText(buffer, TextType::Raw, "", "");
        const auto back = readText(buffer);
        REQUIRE(back.has_value());
        CHECK(back->message == " ");
    }
}

TEST_CASE("the door tells an old client and a new one apart, and says so by number") {
    const BedrockGate gate{{kNativeBedrockProtocol}, BedrockFuturePolicy::Refuse};

    CHECK(gate.verdict(kNativeBedrockProtocol).kind == BedrockVerdict::Kind::Speak);
    CHECK(gate.verdict(kNativeBedrockProtocol).allowed());
    CHECK(gate.verdict(924).kind == BedrockVerdict::Kind::TooOld);
    CHECK_FALSE(gate.verdict(924).allowed());
    CHECK(gate.verdict(kNativeBedrockProtocol + 1).kind == BedrockVerdict::Kind::TooNew);

    // What each one reads is about the other side, not about itself.
    CHECK(gate.refusalFor(gate.verdict(924), "1.26.40").find("Update your game") != std::string::npos);
    CHECK(gate.refusalFor(gate.verdict(9999), "1.26.40").find("does not speak your version") != std::string::npos);

    SUBCASE("a newer client is let in when the operator asked for that, and the log is told once") {
        const BedrockGate warning{{kNativeBedrockProtocol}, BedrockFuturePolicy::Warn};
        const BedrockVerdict verdict = warning.verdict(9999);
        CHECK(verdict.kind == BedrockVerdict::Kind::Unknown);
        CHECK(verdict.warn);
        CHECK(verdict.allowed());

        const BedrockGate quiet{{kNativeBedrockProtocol}, BedrockFuturePolicy::Accept};
        CHECK(quiet.verdict(9999).allowed());
        CHECK_FALSE(quiet.verdict(9999).warn);
        // Old is old whatever the policy for new ones says.
        CHECK_FALSE(quiet.verdict(924).allowed());
    }

    SUBCASE("a word that is not a policy is refused rather than guessed") {
        CHECK(parseBedrockFuturePolicy("warn") == BedrockFuturePolicy::Warn);
        CHECK_FALSE(parseBedrockFuturePolicy("maybe").has_value());
    }
}

TEST_CASE("a form goes out as json and its answer comes back as a number") {
    const std::vector<std::string> buttons{"First button", "Second button"};
    const std::string json = simpleFormJson("Test Menu", "", buttons);
    CHECK(json == R"({"type":"form","title":"Test Menu","content":"","buttons":[{"text":"First button"},)"
                  R"({"text":"Second button"}]})");

    io::ByteBuffer request;
    writeModalFormRequest(request, 7, json);
    CHECK(readVarUInt(request) == 7);
    CHECK(readString(request) == json);

    // What a client sends back: the button it pressed, as the json number its UI writes.
    io::ByteBuffer answer;
    writeVarUInt(answer, 7);
    answer.writeByte(1);
    writeString(answer, "1\n");
    answer.writeByte(0);
    const auto back = readModalFormResponse(answer);
    REQUIRE(back.has_value());
    CHECK(back->formId == 7);
    REQUIRE(back->data.has_value());
    CHECK(*back->data == "1\n");

    SUBCASE("a dismissed form answers with nothing at all, which is not a button") {
        io::ByteBuffer dismissed;
        writeVarUInt(dismissed, 7);
        dismissed.writeByte(0);
        dismissed.writeByte(1);
        dismissed.writeByte(0);
        const auto empty = readModalFormResponse(dismissed);
        REQUIRE(empty.has_value());
        CHECK(empty->formId == 7);
        CHECK_FALSE(empty->data.has_value());
    }
}

TEST_CASE("the generated block tables say what each client already has") {
    // Derived by running tools/bedrock-codec/block-palette.py against each version's palette, not
    // recalled: every number here is the position that palette gives the block, read back out of
    // it. They are here so that regenerating against the wrong version, whose order is different,
    // fails loudly instead of drawing the wrong blocks.
    CHECK(bedrockBlockIds2650().size() == kJavaBlockStates);
    CHECK(bedrockBlockId(0, kNativeBedrockProtocol) == 17025);    // minecraft:air
    CHECK(bedrockBlockId(1, kNativeBedrockProtocol) == 3317);     // minecraft:stone
    CHECK(bedrockBlockId(10, kNativeBedrockProtocol) == 13456);   // minecraft:dirt
    CHECK(bedrockBlockId(88, kNativeBedrockProtocol) == 17901);   // minecraft:bedrock
    CHECK(bedrockBlockId(89, kNativeBedrockProtocol) == 12753);   // minecraft:water, at its full depth
    CHECK(bedrockBlockId(5474, kNativeBedrockProtocol) == 1861);  // minecraft:oak_stairs, one way round
    CHECK(bedrockBlockId(167, kNativeBedrockProtocol) == 20921);  // minecraft:poplar_log, upright
    CHECK(bedrockBlockId(2436, kNativeBedrockProtocol) == 19992); // minecraft:white_wool_stairs

    SUBCASE("each protocol reads its own table, and they do not agree") {
        // The palettes are ordered by the hash of the name and grow between versions, so the same
        // block sits somewhere else in each. Two tables agreeing everywhere would mean one is wrong.
        CHECK(bedrockBlockIdsFor(kNativeBedrockProtocol).size() == kJavaBlockStates);
        CHECK(&bedrockBlockIdsFor(kNativeBedrockProtocol) == &bedrockBlockIds2650());
        CHECK(&bedrockBlockIdsFor(2169) == &bedrockBlockIds2640());
        CHECK(&bedrockBlockIdsFor(2168) == &bedrockBlockIds2640());
        CHECK(&bedrockBlockIdsFor(1001) == &bedrockBlockIds2630());
        // Anything newer than this build knows reads the newest table, which is the only answer
        // that can be right for a client from the future.
        CHECK(&bedrockBlockIdsFor(kNativeBedrockProtocol + 1) == &bedrockBlockIds2650());
        CHECK(bedrockBlockIds2630() != bedrockBlockIds2650());
    }

    SUBCASE("an older palette keeps the blocks whose states only grew") {
        // 26.50 gave stairs a corner of their own; 26.40 works it out and has no such state, so
        // the stair is found without it rather than turning into a hole.
        CHECK(bedrockBlockId(5474, 2169) == 1551);
        CHECK(bedrockBlockId(5474, 1001) == 1523);
    }

    SUBCASE("a state a version never had is a hole, not another block") {
        // Wool stairs are newer than 26.40. Air is honest where a guess would be a lie, and the
        // generator counts them out loud when it writes the table.
        CHECK(bedrockBlockId(2436, 2169) == bedrockAirId(2169));
        CHECK(bedrockAirId(2169) == 13629);
        CHECK(bedrockBlockId(-1, kNativeBedrockProtocol) == bedrockAirId(kNativeBedrockProtocol));
        CHECK(bedrockBlockId(static_cast<std::int32_t>(kJavaBlockStates), kNativeBedrockProtocol) ==
              bedrockAirId(kNativeBedrockProtocol));
    }

    SUBCASE("every state lands inside the palette it was generated from") {
        constexpr std::uint16_t kNativePaletteSize = 22091;
        for (const std::uint16_t id : bedrockBlockIds2650()) {
            REQUIRE(id < kNativePaletteSize);
        }
        constexpr std::uint16_t kOldestPaletteSize = 16913;
        for (const std::uint16_t id : bedrockBlockIds2630()) {
            REQUIRE(id < kOldestPaletteSize);
        }
    }
}

TEST_CASE("a subchunk goes out in the shape a client unpacks") {
    // One block of stone at a corner that tells every axis apart, and air everywhere else. If x and
    // z were swapped, or the nesting were Java's, this lands somewhere else and the check fails.
    constexpr std::int32_t kAir = 0;
    constexpr std::int32_t kStone = 1;
    constexpr std::size_t kX = 1;
    constexpr std::size_t kY = 2;
    constexpr std::size_t kZ = 3;
    std::array<std::int32_t, kSubChunkBlocks> blocks{};
    blocks.fill(kAir);
    blocks[(kY << 8U) | (kZ << 4U) | kX] = kStone;

    io::ByteBuffer body;
    writeSubChunk(body, blocks, 0, kNativeBedrockProtocol);

    CHECK(body.readByte() == 9); // The version that names the subchunk,
    CHECK(body.readByte() == 1); // one layer of blocks,
    CHECK(body.readByte() == 0); // and this is the bottom one.

    const std::uint8_t header = body.readByte();
    CHECK((header & 1U) == 1U); // The numbers are the client's own,
    const std::uint8_t bits = header >> 1U;
    CHECK(bits == 1); // and two blocks need one bit each.

    // The words, then the palette behind them.
    const std::size_t perWord = 32U / bits;
    const std::size_t words = kSubChunkBlocks / perWord;
    std::vector<std::uint32_t> packed;
    for (std::size_t at = 0; at < words; ++at) {
        packed.push_back(static_cast<std::uint32_t>(readIntLE(body)));
    }
    const std::int32_t entries = readVarInt(body);
    REQUIRE(entries == 2);
    std::vector<std::int32_t> palette;
    for (std::int32_t at = 0; at < entries; ++at) {
        palette.push_back(readVarInt(body));
    }
    CHECK_FALSE(body.failed());
    CHECK(body.readableBytes() == 0);
    CHECK(palette[0] == bedrockBlockId(kAir, kNativeBedrockProtocol));
    CHECK(palette[1] == bedrockBlockId(kStone, kNativeBedrockProtocol));

    // Unpack the way a client does, and find the stone where it was put.
    const auto blockAt = [&](const std::size_t x, const std::size_t y, const std::size_t z) {
        const std::size_t over = (x << 8U) | (z << 4U) | y;
        const std::uint32_t word = packed[over / perWord];
        const std::size_t slot = over % perWord;
        return palette[(word >> (slot * bits)) & 1U];
    };
    CHECK(blockAt(kX, kY, kZ) == bedrockBlockId(kStone, kNativeBedrockProtocol));
    CHECK(blockAt(kZ, kY, kX) == bedrockBlockId(kAir, kNativeBedrockProtocol)); // Not with x and z the other way round,
    CHECK(blockAt(kX, kZ, kY) == bedrockBlockId(kAir, kNativeBedrockProtocol)); // nor with y and z swapped.
    CHECK(blockAt(0, 0, 0) == bedrockBlockId(kAir, kNativeBedrockProtocol));
}

TEST_CASE("a creative group sits in a tab, and its number is a word or a byte by version") {
    // Two mistakes this has already made. The width moved at 2168, and writing the wrong one leaves
    // everything after it three bytes out of place. And the number itself has to name a tab: `all`
    // is a word for the whole menu, not somewhere a group can sit, and a client handed it reads no
    // further and hangs up.
    constexpr std::int32_t kItemsTab = 4;

    io::ByteBuffer older;
    writeCreativeContent(older, kCreativeCategoryByteProtocol - 1);
    REQUIRE(readVarUInt(older) == 1);
    CHECK(readIntLE(older) == kItemsTab);
    CHECK(readString(older) == "itemGroup.name.basalt");

    io::ByteBuffer newer;
    writeCreativeContent(newer, kCreativeCategoryByteProtocol);
    REQUIRE(readVarUInt(newer) == 1);
    CHECK(newer.readByte() == kItemsTab);
    CHECK(readString(newer) == "itemGroup.name.basalt");
}

TEST_CASE("the biome list names the one every chunk points at") {
    // A column's biome palette is a single entry saying zero, so a list saying there are no biomes
    // at all contradicts every chunk this server sends. An empty list is a claim, not a stub.
    io::ByteBuffer body;
    writeBiomeDefinitionList(body);

    REQUIRE(readVarUInt(body) == 1);
    CHECK(readShortLE(body) == 0); // Its name is the first string below,
    // and minus one where an id would go. 827 made this a plain number and 844 cut the climate
    // down to five fields: written in 800's shape, with an optional id and eight floats, this is a
    // packet a client hangs up on.
    CHECK(readShortLE(body) == 0xffff);
    for (int field = 0; field < 5; ++field) {
        static_cast<void>(readFloatLE(body));
    }
    static_cast<void>(readIntLE(body)); // The map water colour,
    static_cast<void>(body.readByte()); // whether it rains,
    CHECK(body.readByte() == 0);        // no tags,
    CHECK(body.readByte() == 0);        // and nothing about how it is generated.

    REQUIRE(readVarUInt(body) == 1);
    CHECK(readString(body) == "plains");
    CHECK_FALSE(body.failed());
    CHECK(body.readableBytes() == 0);
}

TEST_CASE("the creative menu reads back exactly as long as it was written") {
    // Walked the way a client walks it: the length in front of an item's trailing data is what
    // makes a mistake anywhere inside it show up as bytes left over at the end.
    io::ByteBuffer body;
    writeCreativeContent(body, kNativeBedrockProtocol);

    const auto readInstance = [&body] {
        const std::int32_t id = readVarInt(body);
        static_cast<void>(readShortLE(body));
        static_cast<void>(readVarUInt(body));
        static_cast<void>(readVarInt(body));
        const std::uint32_t trailing = readVarUInt(body);
        REQUIRE(trailing <= body.readableBytes());
        static_cast<void>(body.readSpan(trailing));
        return id;
    };

    REQUIRE(readVarUInt(body) == 1);
    static_cast<void>(body.readByte());
    CHECK(readString(body) == "itemGroup.name.basalt");
    CHECK(readInstance() == 0); // The group's icon is nothing.

    const std::uint32_t items = readVarUInt(body);
    CHECK(items == bedrockItemPalette().size());
    for (std::uint32_t at = 0; at < items; ++at) {
        CHECK(readVarUInt(body) == at + 1); // Its number in the menu,
        CHECK(readInstance() == static_cast<std::int32_t>(at) + 1);
        CHECK(readVarUInt(body) == 0); // and the one group everything is in.
    }

    CHECK_FALSE(body.failed());
    CHECK(body.readableBytes() == 0);
}

TEST_CASE("a recipe list reads back the way a client walks it") {
    // Every field of a recipe is variable length, so a mistake in any of them shows up as bytes
    // left over at the end rather than as a wrong number in the middle.
    BedrockRecipe table;
    table.id = "minecraft:crafting_table";
    table.shaped = true;
    table.width = 2;
    table.height = 2;
    table.ingredients = {BedrockIngredient{.name = "minecraft:planks", .tag = true},
                         BedrockIngredient{.name = "minecraft:planks", .tag = true},
                         BedrockIngredient{.name = "minecraft:planks", .tag = true}, BedrockIngredient{}};
    table.result = BedrockItemStack{.runtimeId = 7, .count = 1};
    table.netId = 1;

    BedrockRecipe buttons;
    buttons.id = "minecraft:oak_button";
    buttons.ingredients = {BedrockIngredient{.name = "minecraft:oak_planks", .aux = 0, .count = 1}};
    buttons.result = BedrockItemStack{.runtimeId = 9, .count = 1};
    buttons.netId = 2;

    const std::array<BedrockRecipe, 2> recipes{table, buttons};
    io::ByteBuffer body;
    writeCraftingData(body, recipes, kNativeBedrockProtocol);

    const auto readIngredient = [&body] {
        const std::uint32_t kind = readVarUInt(body);
        if (kind == 0) {
            static_cast<void>(readVarInt(body)); // Any of them, which is what an empty cell asks for,
            static_cast<void>(readVarInt(body)); // and none of it.
            return std::string{};
        }
        const std::string named = readString(body);
        const std::string name = readString(body);
        static_cast<void>(readVarInt(body)); // Which of them,
        static_cast<void>(readVarInt(body)); // and how many.
        return named == "item_tag" ? "#" + name : name;
    };
    const auto readResult = [&body] {
        const std::int32_t id = readVarInt(body);
        static_cast<void>(readShortLE(body));
        static_cast<void>(readVarUInt(body));
        static_cast<void>(readVarInt(body));
        const std::uint32_t trailing = readVarUInt(body);
        REQUIRE(trailing <= body.readableBytes());
        static_cast<void>(body.readSpan(trailing));
        return id;
    };
    const auto readTail = [&body] {
        static_cast<void>(body.readSpan(16)); // The uuid,
        CHECK(readString(body) == "crafting_table");
        static_cast<void>(readVarInt(body)); // its priority,
    };
    const auto readUnlocking = [&body] {
        CHECK(body.readByte() == 1);
        CHECK(readVarInt(body) == 0);
        CHECK(body.readByte() == 1);
        CHECK(readVarUInt(body) == 0);
    };

    REQUIRE(readVarUInt(body) == 1); // One shaped recipe,
    CHECK(readString(body) == "minecraft:crafting_table");
    CHECK(readVarInt(body) == 2);
    CHECK(readVarInt(body) == 2);
    REQUIRE(readVarUInt(body) == 4);
    CHECK(readIngredient() == "#minecraft:planks");
    static_cast<void>(readIngredient());
    static_cast<void>(readIngredient());
    CHECK(readIngredient().empty());
    REQUIRE(readVarUInt(body) == 1);
    CHECK(readResult() == 7);
    readTail();
    CHECK(body.readByte() == 1); // mirrored is the same recipe,
    readUnlocking();
    CHECK(readVarUInt(body) == 1);

    REQUIRE(readVarUInt(body) == 1); // and one shapeless,
    CHECK(readString(body) == "minecraft:oak_button");
    REQUIRE(readVarUInt(body) == 1);
    CHECK(readIngredient() == "minecraft:oak_planks");
    REQUIRE(readVarUInt(body) == 1);
    CHECK(readResult() == 9);
    readTail();
    readUnlocking();
    CHECK(readVarUInt(body) == 2);

    // The nine kinds this server sends none of, and the flag that replaces what the client had.
    for (int kind = 0; kind < 9; ++kind) {
        CHECK(readVarUInt(body) == 0);
    }
    CHECK(body.readByte() == 1);
    CHECK_FALSE(body.failed());
    CHECK(body.readableBytes() == 0);
}

TEST_CASE("a villager's book is one tag a client can walk to the end of") {
    // Nothing here reads this back in the server, so the test is the only thing that walks it: a
    // compound left open or a list counted wrong shows up as bytes left over.
    BedrockTrade wheat;
    wheat.costA = BedrockTradeItem{.name = "minecraft:wheat", .count = 20};
    wheat.sell = BedrockTradeItem{.name = "minecraft:emerald", .count = 1};
    wheat.maxUses = 16;
    wheat.experience = 2;

    BedrockTrade bread;
    bread.costA = BedrockTradeItem{.name = "minecraft:emerald", .count = 1};
    bread.costB = BedrockTradeItem{.name = "minecraft:wheat", .count = 3};
    bread.sell = BedrockTradeItem{.name = "minecraft:bread", .count = 6};
    bread.tier = 1;

    const std::array<BedrockTrade, 2> trades{wheat, bread};
    io::ByteBuffer body;
    writeUpdateTrade(body, 5, 1, 42, 1, "Villager", trades);

    CHECK(body.readByte() == 5);
    CHECK(body.readByte() == 15); // The window kind a villager opens,
    CHECK(readVarInt(body) == 0); // with no slots of its own,
    CHECK(readVarInt(body) == 1); // at its second rank,
    CHECK(readVarLong(body) == 42);
    CHECK(readVarLong(body) == 1);
    CHECK(readString(body) == "Villager");
    CHECK(body.readByte() == 1);
    CHECK(body.readByte() == 1);

    // The tag itself, walked the way a client walks one: a name, then whatever that kind holds.
    std::int32_t names = 0;
    std::int32_t compounds = 0;
    const std::function<void()> walk = [&] {
        while (!body.failed()) {
            const std::uint8_t kind = body.readByte();
            if (kind == 0) {
                return;
            }
            static_cast<void>(readString(body));
            ++names;
            switch (kind) {
            case 0x01:
                static_cast<void>(body.readByte());
                break;
            case 0x02:
                static_cast<void>(readShortLE(body));
                break;
            case 0x03:
                static_cast<void>(readVarInt(body));
                break;
            case 0x05:
                static_cast<void>(readFloatLE(body));
                break;
            case 0x08:
                static_cast<void>(readString(body));
                break;
            case 0x09: {
                REQUIRE(body.readByte() == 0x0a); // Lists of compounds are the only kind here.
                const std::int32_t entries = readVarInt(body);
                for (std::int32_t at = 0; at < entries; ++at) {
                    ++compounds;
                    walk();
                }
                break;
            }
            case 0x0a:
                ++compounds;
                walk();
                break;
            default:
                FAIL("a tag kind this book should not carry: " << static_cast<std::int32_t>(kind));
            }
        }
    };
    REQUIRE(body.readByte() == 0x0a); // The root,
    CHECK(readString(body).empty());  // which has no name.
    walk();

    // Two trades and five ranks in the two lists, plus the item tags: three in the second trade and
    // two in the first, whose second cost is the one tag that carries nothing.
    CHECK(compounds == 2 + 5 + 3 + 3);
    // The two lists, fourteen fields a trade, one number a rank, and three names an item that has
    // something in it.
    CHECK(names == 2 + (2 * 14) + 5 + (5 * 3));
    CHECK_FALSE(body.failed());
    CHECK(body.readableBytes() == 0);
}

TEST_CASE("a sign's editor sends back what a client typed into it") {
    basalt::wire::io::ByteBuffer written;
    basalt::wire::bedrock::writeSignData(written, -5, 70, 12, "one\ntwo", "back", false);

    basalt::wire::io::ByteBuffer body;
    body.writeBytes(written.readable());
    const std::optional<basalt::wire::bedrock::BedrockSignEdit> edit = basalt::wire::bedrock::readSignEdit(body);

    REQUIRE(edit.has_value());
    CHECK(edit->x == -5);
    CHECK(edit->y == 70);
    CHECK(edit->z == 12);
    CHECK(edit->text == "one\ntwo");
}

TEST_CASE("a locator bar line carries every field a client reads, in both shapes") {
    namespace bedrock = basalt::wire::bedrock;

    bedrock::BedrockWaypoint waypoint;
    waypoint.groupHigh = 0x0123456789abcdefULL;
    waypoint.groupLow = 0xfedcba9876543210ULL;
    waypoint.x = 12.5F;
    waypoint.y = 71.0F;
    waypoint.z = -34.25F;
    waypoint.dimension = bedrock::BedrockDimension::Nether;
    waypoint.colour = 0xff0000;
    waypoint.distance = 10.0F;
    waypoint.entityId = 77;

    basalt::wire::io::ByteBuffer written;
    bedrock::writeLocatorBar(written, waypoint, bedrock::BedrockWaypointAction::Add, 2168);

    basalt::wire::io::ByteBuffer read;
    read.writeBytes(written.readable());
    CHECK(bedrock::readVarUInt(read) == 1);
    CHECK(bedrock::readLongLE(read) == static_cast<std::int64_t>(waypoint.groupHigh));
    CHECK(bedrock::readLongLE(read) == static_cast<std::int64_t>(waypoint.groupLow));
    // Every field is being set at once, which is what an arrival says.
    CHECK(bedrock::readIntLE(read) == 0b111111);
    CHECK(read.readByte() == 1);
    CHECK(read.readByte() == 1); // Drawn.
    CHECK(read.readByte() == 1);
    CHECK(bedrock::readFloatLE(read) == 12.5F);
    CHECK(bedrock::readFloatLE(read) == 71.0F);
    CHECK(bedrock::readFloatLE(read) == -34.25F);
    CHECK(bedrock::readVarInt(read) == 1);
    CHECK(read.readByte() == 1);
    // Near enough to be the first dot, drawn at its full size.
    CHECK(bedrock::readString(read) == "textures/ui/locator_bar_dot_0");
    CHECK(read.readByte() == 1);
    CHECK(bedrock::readFloatLE(read) == 1.0F);
    CHECK(bedrock::readFloatLE(read) == 1.0F);
    CHECK(read.readByte() == 1);
    // The alpha is the writer's, since a dot drawn see-through is a dot nobody sees.
    CHECK(static_cast<std::uint32_t>(bedrock::readIntLE(read)) == 0xff'ff0000U);
    CHECK(read.readByte() == 1);
    CHECK(read.readByte() == 1); // The client moves it itself,
    CHECK(read.readByte() == 1);
    CHECK(bedrock::readVarLong(read) == 77); // following this entity.
    CHECK(read.readByte() == 1);             // Arriving.
    CHECK(!read.failed());

    // 944 has no textures: the dot is a number, and the far ones are drawn smaller by the client.
    waypoint.distance = 400.0F;
    basalt::wire::io::ByteBuffer old;
    bedrock::writeLocatorBar(old, waypoint, bedrock::BedrockWaypointAction::Update, 944);
    basalt::wire::io::ByteBuffer back;
    back.writeBytes(old.readable());
    CHECK(bedrock::readVarUInt(back) == 1);
    CHECK(bedrock::readLongLE(back) == static_cast<std::int64_t>(waypoint.groupHigh));
    CHECK(bedrock::readLongLE(back) == static_cast<std::int64_t>(waypoint.groupLow));
    CHECK(bedrock::readIntLE(back) == 0b111111);
    CHECK(back.readByte() == 1);
    CHECK(back.readByte() == 1);
    CHECK(back.readByte() == 1);
    CHECK(bedrock::readFloatLE(back) == 12.5F);
    CHECK(bedrock::readFloatLE(back) == 71.0F);
    CHECK(bedrock::readFloatLE(back) == -34.25F);
    CHECK(bedrock::readVarInt(back) == 1);
    CHECK(back.readByte() == 1);
    CHECK(bedrock::readIntLE(back) == 5);
    CHECK(back.readByte() == 1);
    CHECK(static_cast<std::uint32_t>(bedrock::readIntLE(back)) == 0xff'ff0000U);
    CHECK(back.readByte() == 1);
    CHECK(back.readByte() == 1);
    CHECK(back.readByte() == 1);
    CHECK(bedrock::readVarLong(back) == 77);
    CHECK(back.readByte() == 3); // Moved rather than added.
    CHECK(!back.failed());
}

TEST_CASE("a client's complaint about a packet names the packet and the field") {
    namespace bedrock = basalt::wire::bedrock;

    basalt::wire::io::ByteBuffer written;
    // parity: both numbers go out one below their place in the list, so a malformed packet is
    // nought and a warning is nought as well.
    bedrock::writeVarInt(written, 0);
    bedrock::writeVarInt(written, 2);
    bedrock::writeVarInt(written, 341);
    bedrock::writeString(written, "Unable to read LocatorBarPacket");

    basalt::wire::io::ByteBuffer read;
    read.writeBytes(written.readable());
    const std::optional<bedrock::BedrockPacketViolation> violation = bedrock::readPacketViolationWarning(read);

    REQUIRE(violation.has_value());
    CHECK(violation->type == 0);
    CHECK(violation->severity == 2);
    CHECK(violation->packetId == 341);
    CHECK(violation->context == "Unable to read LocatorBarPacket");

    // A short line is not one of these, and nothing is read out of what is not there.
    basalt::wire::io::ByteBuffer cut;
    cut.writeBytes(std::span{written.readable().data(), 2});
    CHECK(!bedrock::readPacketViolationWarning(cut).has_value());
}
