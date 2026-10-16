#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/multiversion/LoginShapes.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>

#include <doctest.h>

using namespace basalt::wire;
namespace shapes = multiversion::loginShapes;
namespace versions = multiversion::protocolVersions;

TEST_CASE("the login of each version before 1.20.2 reads back to the native shape") {
    const common::Uuid id{0x0123456789abcdefULL, 0xfedcba9876543210ULL};
    for (const std::int32_t protocol : {versions::kV1_8, versions::kV1_12_2, versions::kV1_16_2, versions::kV1_18_2,
                                        versions::kV1_19, versions::kV1_19_1, versions::kV1_19_3, versions::kV1_20}) {
        io::ByteBuffer start;
        shapes::writeLoginStart(start, protocol, "Steve", id);
        const auto name = shapes::readClientLoginName(protocol, start);
        REQUIRE(name);
        CHECK(*name == "Steve");

        protocol::login::LoginFinishedPacket finished;
        finished.gameProfile.id = id;
        finished.gameProfile.name = "Steve";
        io::ByteBuffer shaped;
        shapes::writeLoginSuccess(shaped, protocol, finished);
        const auto native = shapes::loginFinishedToNative(protocol, shaped.readable());
        REQUIRE(native);
        io::ByteBuffer again{native->size()};
        again.writeBytes(*native);
        const auto parsed = protocol::login::LoginFinishedPacket::read(again);
        REQUIRE(parsed);
        CHECK(parsed->gameProfile.id == id);
        CHECK(parsed->gameProfile.name == "Steve");
    }
}

TEST_CASE("a chain reads an old client's login start and writes its login success the way that version shapes them") {
    const common::Uuid id{0x0123456789abcdefULL, 0xfedcba9876543210ULL};
    for (const std::int32_t protocol : {versions::kV1_8, versions::kV1_16_2, versions::kV1_18_2, versions::kV1_20}) {
        const auto chain = multiversion::java::javaTranslators::chainFor(protocol);
        REQUIRE(chain);
        io::ByteBuffer start;
        shapes::writeLoginStart(start, protocol, "Steve", id);
        std::vector<std::uint8_t> native;
        chain->chain().serverbound(
            protocol::ProtocolState::Login, 0, start.readable(),
            [&](std::int32_t, std::span<const std::uint8_t> body) { native.assign(body.begin(), body.end()); });
        io::ByteBuffer view{native.size()};
        view.writeBytes(native);
        const auto read = protocol::login::LoginStartPacket::read(view);
        REQUIRE(read);
        CHECK(read->name == "Steve");

        protocol::login::LoginFinishedPacket finished;
        finished.gameProfile.id = id;
        finished.gameProfile.name = "Steve";
        io::ByteBuffer nativeSuccess;
        finished.write(nativeSuccess);
        std::vector<std::uint8_t> shaped;
        chain->chain().clientbound(
            protocol::ProtocolState::Login, 2, nativeSuccess.readable(),
            [&](std::int32_t, std::span<const std::uint8_t> body) { shaped.assign(body.begin(), body.end()); });
        io::ByteBuffer expected;
        shapes::writeLoginSuccess(expected, protocol, finished);
        CHECK(shaped == std::vector<std::uint8_t>{expected.readable().begin(), expected.readable().end()});
    }
}
