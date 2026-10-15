#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/bedrock/BedrockBatch.hpp>
#include <basalt/wire/bedrock/BedrockBiomes.hpp>
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
#include <memory>
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

    std::vector<std::uint8_t> second;
    sender.encrypt(plain, second);
    CHECK(second != encrypted);
    REQUIRE(receiver.decrypt(second, decrypted));
    CHECK(std::string(decrypted.begin(), decrypted.end()) == text);

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

    harness.sendToServer(BedrockPacket::ClientToServerHandshake, {});
    REQUIRE(harness.take(BedrockPacket::PlayStatus).has_value());
    REQUIRE(harness.take(BedrockPacket::ResourcePacksInfo).has_value());

    io::ByteBuffer response;
    writeVarUInt(response, 2);
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

TEST_CASE("a subchunk goes out in the shape a client unpacks") {
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

    CHECK(body.readByte() == 9);
    CHECK(body.readByte() == 1);
    CHECK(body.readByte() == 0);

    const std::uint8_t header = body.readByte();
    CHECK((header & 1U) == 1U);
    const std::uint8_t bits = header >> 1U;
    CHECK(bits == 1);

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
    CHECK(blockAt(kZ, kY, kX) == bedrockBlockId(kAir, kNativeBedrockProtocol));
    CHECK(blockAt(kX, kZ, kY) == bedrockBlockId(kAir, kNativeBedrockProtocol));
    CHECK(blockAt(0, 0, 0) == bedrockBlockId(kAir, kNativeBedrockProtocol));
}

TEST_CASE("the biome list reads back whole, every name and tag pointing into its strings") {
    io::ByteBuffer body;
    writeBiomeDefinitionList(body);

    const std::uint32_t count = readVarUInt(body);
    REQUIRE(count == bedrockBiomeDefinitions().size());
    std::vector<std::uint16_t> names;
    std::uint16_t highest = 0;
    for (std::uint32_t biome = 0; biome < count; ++biome) {
        names.push_back(readShortLE(body));
        highest = std::max(highest, names.back());
        CHECK(readShortLE(body) == 0xffff);
        for (int field = 0; field < 5; ++field) {
            static_cast<void>(readFloatLE(body));
        }
        static_cast<void>(readIntLE(body));
        static_cast<void>(body.readByte());
        if (body.readByte() != 0) {
            const std::uint32_t tags = readVarUInt(body);
            for (std::uint32_t tag = 0; tag < tags; ++tag) {
                highest = std::max(highest, readShortLE(body));
            }
        }
        CHECK(body.readByte() == 0);
    }
    const std::uint32_t strings = readVarUInt(body);
    CHECK(highest < strings);
    std::vector<std::string> table;
    for (std::uint32_t at = 0; at < strings; ++at) {
        table.push_back(readString(body));
    }
    CHECK_FALSE(body.failed());
    CHECK(body.readableBytes() == 0);
    CHECK(std::ranges::find(table, "minecraft:plains") != table.end());
    CHECK(table[names.front()] == bedrockBiomeDefinitions().front().name);
}

TEST_CASE("the creative menu reads back exactly as long as it was written") {
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
    CHECK(readInstance() == 0);

    const std::uint32_t items = readVarUInt(body);
    CHECK(items == bedrockItemPalette().size());
    for (std::uint32_t at = 0; at < items; ++at) {
        CHECK(readVarUInt(body) == at + 1);
        CHECK(readInstance() == static_cast<std::int32_t>(at) + 1);
        CHECK(readVarUInt(body) == 0);
    }

    CHECK_FALSE(body.failed());
    CHECK(body.readableBytes() == 0);
}

TEST_CASE("a recipe list reads back the way a client walks it") {
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
            static_cast<void>(readVarInt(body));
            static_cast<void>(readVarInt(body));
            return std::string{};
        }
        const std::string named = readString(body);
        const std::string name = readString(body);
        static_cast<void>(readVarInt(body));
        static_cast<void>(readVarInt(body));
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
        static_cast<void>(body.readSpan(16));
        CHECK(readString(body) == "crafting_table");
        static_cast<void>(readVarInt(body));
    };
    const auto readUnlocking = [&body] {
        CHECK(body.readByte() == 1);
        CHECK(readVarInt(body) == 0);
        CHECK(body.readByte() == 1);
        CHECK(readVarUInt(body) == 0);
    };

    REQUIRE(readVarUInt(body) == 1);
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
    CHECK(body.readByte() == 1);
    readUnlocking();
    CHECK(readVarUInt(body) == 1);

    REQUIRE(readVarUInt(body) == 1);
    CHECK(readString(body) == "minecraft:oak_button");
    REQUIRE(readVarUInt(body) == 1);
    CHECK(readIngredient() == "minecraft:oak_planks");
    REQUIRE(readVarUInt(body) == 1);
    CHECK(readResult() == 9);
    readTail();
    readUnlocking();
    CHECK(readVarUInt(body) == 2);

    REQUIRE(readVarUInt(body) == kCartographyRecipeCount);
    for (std::int32_t index = 0; index < kCartographyRecipeCount; ++index) {
        static_cast<void>(body.readSpan(16));
        CHECK(readVarUInt(body) == static_cast<std::uint32_t>(kFirstCartographyRecipeNetId + index));
    }
    for (int kind = 0; kind < 8; ++kind) {
        CHECK(readVarUInt(body) == 0);
    }
    CHECK(body.readByte() == 1);
    CHECK_FALSE(body.failed());
    CHECK(body.readableBytes() == 0);
}
