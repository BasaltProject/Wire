#include <basalt/wire/bedrock/BedrockBatch.hpp>
#include <basalt/wire/bedrock/BedrockConnection.hpp>
#include <basalt/wire/bedrock/BedrockGate.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockTransport.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>
#include <basalt/wire/bedrock/NetherNetIdentity.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <doctest.h>

using namespace basalt::wire;
using namespace basalt::wire::bedrock;

TEST_CASE("a NetherNet identity proves the key that signed the fingerprint") {
    const auto key = EcKeyPair::generate();
    REQUIRE(key.has_value());
    const std::string digest =
        "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99";

    const auto identity = buildNetherNetAnswerIdentity(*key, "example.org", digest);
    REQUIRE(identity.has_value());

    const auto proven = verifyNetherNetOfferIdentity(*identity, "sha-256", digest);
    REQUIRE(proven.has_value());
    const auto expected = key->publicKeyDer();
    CHECK(proven->publicKeyDer == std::vector<std::uint8_t>(expected.begin(), expected.end()));

    CHECK_FALSE(verifyNetherNetOfferIdentity(*identity, "sha-256", "00:11").has_value());
}

namespace {

struct SinkTransport final : BedrockTransport {
    std::vector<std::vector<std::uint8_t>> sent;

    void sendGame(std::span<const std::uint8_t> payload) override { sent.emplace_back(payload.begin(), payload.end()); }

    [[nodiscard]] bool protectsBytes() const noexcept override { return true; }
};

std::vector<std::uint8_t> clientPacket(BedrockPacket packet, std::span<const std::uint8_t> body, bool compressed) {
    io::ByteBuffer encoded;
    writeVarUInt(encoded, static_cast<std::uint32_t>(packet));
    encoded.writeBytes(body);
    BatchSettings settings;
    settings.enabled = compressed;
    io::ByteBuffer batch;
    const std::vector<std::vector<std::uint8_t>> packets{{encoded.readable().begin(), encoded.readable().end()}};
    REQUIRE(packBatch(packets, settings, batch));
    return {batch.readable().begin(), batch.readable().end()};
}

} // namespace

TEST_CASE("every protocol in the window logs in over a transport that protects the bytes, without encryption") {
    for (const int protocol : kBedrockProtocols) {
        CAPTURE(protocol);
        SinkTransport transport;
        auto serverKeys = std::make_shared<EcKeyPair>(*EcKeyPair::generate());
        BedrockConnection connection{transport, serverKeys, "1.26.60",
                                     BedrockGate{BedrockGate::buildSupport(), BedrockFuturePolicy::Refuse}};
        std::string loggedInAs;
        bool ready = false;
        connection.onLogin([&loggedInAs](const LoginRequest& login) { loggedInAs = login.name; });
        connection.onReady([&ready] { ready = true; });

        io::ByteBuffer request;
        request.writeInt(protocol);
        connection.receive(clientPacket(BedrockPacket::RequestNetworkSettings, request.readable(), false));
        REQUIRE(transport.sent.size() == 1);

        EcKeyPair clientKeys = *EcKeyPair::generate();
        const std::string identity =
            R"({"identityPublicKey":")" + clientKeys.publicKeyBase64() +
            R"(","extraData":{"displayName":"Steve","identity":"00000000-0000-0000-0000-000000000001","XUID":"2535000000000000"}})";
        common::JsonObject object;
        object.set("AuthenticationType", common::JsonValue::of(static_cast<std::int64_t>(1)));
        object.set("Certificate", common::JsonValue::of(R"({"chain":[")" + *clientKeys.signJwt(identity) + R"("]})"));
        object.set("Token", common::JsonValue::of(std::string{}));
        const std::string authJson = common::writeJson(common::JsonValue::ofObject(object));
        const std::string clientJwt = *clientKeys.signJwt(R"({"DeviceOS":1})");
        io::ByteBuffer login;
        login.writeInt(protocol);
        writeVarUInt(login, static_cast<std::uint32_t>(authJson.size() + clientJwt.size() + 8));
        writeIntLE(login, static_cast<std::int32_t>(authJson.size()));
        login.writeBytes({reinterpret_cast<const std::uint8_t*>(authJson.data()), authJson.size()});
        writeIntLE(login, static_cast<std::int32_t>(clientJwt.size()));
        login.writeBytes({reinterpret_cast<const std::uint8_t*>(clientJwt.data()), clientJwt.size()});
        connection.receive(clientPacket(BedrockPacket::Login, login.readable(), true));
        CHECK(loggedInAs == "Steve");
        CHECK(connection.protocolVersion() == protocol);

        io::ByteBuffer packsDone;
        if (writesNativeShapes(protocol)) {
            writeVarUInt(packsDone, 2);
        } else {
            packsDone.writeByte(3);
        }
        connection.receive(clientPacket(BedrockPacket::ResourcePackClientResponse, packsDone.readable(), true));
        io::ByteBuffer stackDone;
        if (writesNativeShapes(protocol)) {
            writeVarUInt(stackDone, 3);
        } else {
            stackDone.writeByte(4);
        }
        connection.receive(clientPacket(BedrockPacket::ResourcePackClientResponse, stackDone.readable(), true));
        CHECK(ready);
        CHECK(connection.state() == BedrockState::Playing);
    }
}
