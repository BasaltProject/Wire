#include <basalt/wire/protocol/cookie/CookiePackets.hpp>

#include <doctest.h>

using namespace basalt::wire;
using namespace protocol::cookie;

TEST_CASE("cookie codecs retain absent, empty and maximum-size payloads") {
    io::ByteBuffer buffer;
    RequestPacket request{"basalt:route"};
    request.write(buffer);
    CHECK(RequestPacket::read(buffer) == request);
    for (const auto& payload : {std::optional<std::vector<std::uint8_t>>{}, std::optional{std::vector<std::uint8_t>{}},
                                std::optional{std::vector<std::uint8_t>(kMaxPayloadBytes, 42)}}) {
        buffer.clear();
        ResponsePacket response{"basalt:route", payload};
        response.write(buffer);
        CHECK(ResponsePacket::read(buffer) == response);
    }
    buffer.clear();
    StorePacket store{"basalt:route", std::vector<std::uint8_t>(kMaxPayloadBytes, 7)};
    store.write(buffer);
    CHECK(StorePacket::read(buffer) == store);
    buffer.clear();
    store.payload.push_back(1);
    store.write(buffer);
    CHECK(buffer.failed());
}

TEST_CASE("cookie decoder rejects oversized payload declarations before reading bytes") {
    auto buffer = io::ByteBuffer::wrap({3, 'a', ':', 'b', 1, 0x81, 0x28});
    CHECK_FALSE(ResponsePacket::read(buffer));
    buffer = io::ByteBuffer::wrap({3, 'a', ':', 'b', 0x81, 0x28});
    CHECK_FALSE(StorePacket::read(buffer));
    buffer = io::ByteBuffer::wrap({3, 'A', ':', 'b'});
    CHECK_FALSE(RequestPacket::read(buffer));
    buffer = io::ByteBuffer::wrap({3, 'a', ':', 'b', 1, 2, 42});
    CHECK_FALSE(ResponsePacket::read(buffer));
}

TEST_CASE("cookie responses match independent binary vectors and canonical identifiers") {
    io::ByteBuffer buffer;
    ResponsePacket{"a:b", std::nullopt}.write(buffer);
    CHECK(buffer.toVector() == std::vector<std::uint8_t>{3, 'a', ':', 'b', 0});
    buffer.clear();
    ResponsePacket{"a:b", std::vector<std::uint8_t>{}}.write(buffer);
    CHECK(buffer.toVector() == std::vector<std::uint8_t>{3, 'a', ':', 'b', 1, 0});
    buffer = io::ByteBuffer::wrap({3, 'k', 'e', 'y'});
    auto request = RequestPacket::read(buffer);
    REQUIRE(request);
    CHECK(request->key == "minecraft:key");
}
