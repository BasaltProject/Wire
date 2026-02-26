#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/bedrock/BedrockBatch.hpp>
#include <basalt/wire/bedrock/BedrockCipher.hpp>
#include <basalt/wire/bedrock/BedrockConnection.hpp>
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

#include <cstdint>
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
    writeStartGame(start, info);
    harness.connection.sendPacket(BedrockPacket::StartGame, start.readable());
    io::ByteBuffer chunk;
    writeEmptyLevelChunk(chunk, 0, 0, 24, 0);
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
