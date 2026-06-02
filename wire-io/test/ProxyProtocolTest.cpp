#include <basalt/wire/io/ProxyProtocol.hpp>

#include <doctest.h>

#include <string_view>
#include <vector>

namespace proxyProtocol = basalt::wire::io::proxyProtocol;

namespace {
std::vector<std::uint8_t> bytesOf(std::string_view text) { return {text.begin(), text.end()}; }
} // namespace

TEST_CASE("PROXY headers parse incrementally and yield the client address") {
    const auto v1 = bytesOf("PROXY TCP4 203.0.113.7 10.0.0.1 5555 25565\r\nrest");
    const auto partial = proxyProtocol::parse(std::span{v1}.first(10));
    CHECK(partial.status == proxyProtocol::Status::kNeedMore);
    CHECK(partial.needed == 11);
    const auto done = proxyProtocol::parse(v1);
    REQUIRE(done.status == proxyProtocol::Status::kDone);
    CHECK(done.address == "203.0.113.7");
    CHECK(done.port == 5555);
    CHECK(done.consumed == v1.size() - 4);

    const std::vector<std::uint8_t> v2{0x0D, 0x0A, 0x0D, 0x0A, 0x00, 0x0D, 0x0A, 0x51, 0x55, 0x49,
                                       0x54, 0x0A, 0x21, 0x11, 0x00, 0x0C, 192,  0,    2,    9,
                                       10,   0,    0,    1,    0x1F, 0x90, 0x63, 0xDD};
    const auto binary = proxyProtocol::parse(v2);
    REQUIRE(binary.status == proxyProtocol::Status::kDone);
    CHECK(binary.address == "192.0.2.9");
    CHECK(binary.port == 8080);
    CHECK(binary.consumed == v2.size());
    const auto cut = proxyProtocol::parse(std::span{v2}.first(20));
    CHECK(cut.status == proxyProtocol::Status::kNeedMore);
    CHECK(cut.needed == v2.size());

    CHECK(proxyProtocol::parse(bytesOf("GET / HTTP/1.1\r\n")).status == proxyProtocol::Status::kInvalid);
    CHECK(proxyProtocol::parse(bytesOf("PROXY TCP4 1.2.3.4\r\n")).status == proxyProtocol::Status::kInvalid);
}
