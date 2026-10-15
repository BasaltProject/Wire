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
