#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/multiversion/LoginShapes.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
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
