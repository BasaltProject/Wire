#include "../src/HttpsResponseDecoder.hpp"

#include <doctest.h>

#include <limits>
#include <span>
#include <string>
#include <string_view>

using namespace basalt::wire::auth;

namespace {
auto response(std::string_view raw, std::size_t limit = 64) {
    return detail::decodeHttpsResponse({reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size()}, limit);
}
} // namespace

TEST_CASE("HTTP profile replies require complete body boundaries") {
    const auto good = response("HTTP/1.1 200 OK\r\ncontent-length: 2\r\n\r\n{}");
    REQUIRE(good);
    CHECK(good->status == 200);
    CHECK(good->text() == "{}");
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\n{}"));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\n{}"));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nContent-Length: 2x\r\n\r\n{}"));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n{}"));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\n\r\n{}", 1));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nContent-Length: 65\r\n\r\n"));
}

TEST_CASE("chunked replies enforce terminal chunks, delimiters and decoded byte budgets") {
    const auto good =
        response("HTTP/1.1 200 OK\r\ntransfer-encoding: Chunked\r\n\r\n1;x=y\r\n{\r\n1\r\n}\r\n0\r\n\r\n", 2);
    REQUIRE(good);
    CHECK(good->text() == "{}");
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}\r\n"));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}\r\n0\r\n"));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}x\r\n0\r\n\r\n"));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}\r\n0\r\n\r\n", 1));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 2\r\n\r\n0\r\n\r\n"));
    CHECK(response("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nX-Test: synthetic\r\n\r\n"));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nContent-Length: 2\r\n\r\n"));
}

TEST_CASE("HTTP envelopes and malformed status lines cannot bypass response limits") {
    CHECK_FALSE(response("HTTP/1.1 20x OK\r\n\r\n"));
    CHECK_FALSE(response("HTTP/1.1 2000 OK\r\n\r\n"));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\nBroken\r\n\r\n"));
    CHECK_FALSE(response("HTTP/1.1 200 OK\r\n Content-Length: 0\r\n\r\n"));
    CHECK_FALSE(response("", std::numeric_limits<std::size_t>::max()));
    const std::string oversized = "HTTP/1.1 200 OK\r\nX: " + std::string(65536, 'x') + "\r\n\r\n";
    CHECK_FALSE(response(oversized));
}
