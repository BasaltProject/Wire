#include <basalt/wire/io/DnsResolver.hpp>
#include <basalt/wire/io/TcpSocket.hpp>
#include <basalt/wire/io/UdpSocket.hpp>
#include <basalt/wire/io/WorkerPool.hpp>

#include <doctest.h>

#include <array>
#include <chrono>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace basalt::wire::io;
using namespace std::chrono_literals;

namespace {
class DnsFixture {
public:
    enum class Mode { Addresses, NotFound, Silence };

    explicit DnsFixture(Mode mode) : mode_(mode), observed_(firstRequest_.get_future()) {
        auto bound = UdpSocket::bind(0);
        if (!bound)
            throw std::runtime_error("DNS fixture bind failed");
        socket_ = std::move(*bound);
        auto port = socket_.localPort();
        if (!port)
            throw std::runtime_error("DNS fixture port failed");
        servers_ = "127.0.0.1:" + std::to_string(*port);
        worker_ = std::jthread([this](std::stop_token stop) { run(stop); });
    }

    ~DnsFixture() {
        worker_.request_stop();
        worker_.join();
    }

    const std::string& servers() const { return servers_; }

    bool awaitRequest() { return observed_.wait_for(2s) == std::future_status::ready; }

private:
    void run(std::stop_token stop) {
        bool seen = false;
        while (!stop.stop_requested()) {
            std::array<std::uint8_t, 512> bytes{};
            UdpEndpoint from;
            auto received = socket_.receiveFrom(bytes, from, 25);
            if (!received || !*received)
                continue;
            if (!seen) {
                firstRequest_.set_value();
                seen = true;
            }
            if (mode_ == Mode::Silence)
                continue;
            const auto length = **received;
            if (length < 17 || bytes[4] != 0 || bytes[5] != 1)
                continue;
            std::size_t end = 12;
            while (end < length && bytes[end] != 0) {
                if (bytes[end] > 63 || end + bytes[end] + 1 >= length) {
                    end = length;
                    break;
                }
                end += bytes[end] + 1;
            }
            if (end + 5 > length)
                continue;
            const auto type = static_cast<std::uint16_t>((bytes[end + 1] << 8) | bytes[end + 2]);
            end += 5;
            std::vector<std::uint8_t> response(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(end));
            response[2] = 0x81;
            response[3] = mode_ == Mode::NotFound ? 0x83 : 0x80;
            response[6] = response[8] = response[9] = response[10] = response[11] = 0;
            response[7] = mode_ == Mode::Addresses && (type == 1 || type == 28) ? 1 : 0;
            if (response[7]) {
                response.insert(response.end(), {0xc0, 0x0c, 0, static_cast<std::uint8_t>(type), 0, 1, 0, 0, 0, 60, 0,
                                                 static_cast<std::uint8_t>(type == 1 ? 4 : 16)});
                if (type == 1)
                    response.insert(response.end(), {192, 0, 2, 7});
                else
                    response.insert(response.end(), {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7});
            }
            (void)socket_.sendTo(response, from);
        }
    }

    Mode mode_;
    UdpSocket socket_;
    std::string servers_;
    std::promise<void> firstRequest_;
    std::future<void> observed_;
    std::jthread worker_;
};
} // namespace

TEST_CASE("bounded DNS resolves independent A and AAAA responses with requested port") {
    DnsFixture dns{DnsFixture::Mode::Addresses};
    auto endpoints = resolveDns("backend.test", 25565, {std::chrono::steady_clock::now() + 2s, {}, dns.servers()});
    REQUIRE(endpoints);
    REQUIRE(endpoints->size() == 2);
    bool ipv4 = false;
    bool ipv6 = false;
    for (const auto& endpoint : *endpoints) {
        CHECK(endpoint.port() == 25565);
        ipv4 |= endpoint.host() == "192.0.2.7";
        ipv6 |= endpoint.host() == "2001:db8::7";
    }
    CHECK(ipv4);
    CHECK(ipv6);
}

TEST_CASE("DNS stop wakes a long wait and destroys pending queries before returning") {
    DnsFixture dns{DnsFixture::Mode::Silence};
    std::stop_source canceled;
    auto work = std::async(std::launch::async, [&] {
        return resolveDns("canceled.test", 25565,
                          {std::chrono::steady_clock::now() + 10s, canceled.get_token(), dns.servers()});
    });
    REQUIRE(dns.awaitRequest());
    canceled.request_stop();
    CHECK(work.wait_for(1s) == std::future_status::ready);
    auto result = work.get();
    REQUIRE_FALSE(result);
    CHECK(result.error().kind == DnsError::Kind::Canceled);
}
