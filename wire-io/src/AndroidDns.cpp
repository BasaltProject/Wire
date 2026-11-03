#include "AndroidDns.hpp"

#include <basalt/wire/io/EventLoop.hpp>

#include <algorithm>
#include <android/multinetwork.h>
#include <ares.h>
#include <arpa/inet.h>
#include <array>
#include <memory>
#include <string>

namespace basalt::wire::io::detail {
namespace {

struct PendingQuery {
    int fd = -1;

    ~PendingQuery() {
        if (fd >= 0) {
            android_res_cancel(fd);
        }
    }
};

} // namespace

// Android 8+ no longer exposes DNS servers through net.dns properties. A native
// executable has no Java ConnectivityManager to give c-ares, so ask netd through
// the NDK's pollable, cancelable resolver instead of inventing a public DNS server.
std::expected<std::vector<TcpAddress>, DnsError> resolveAndroidDns(std::string_view host, std::uint16_t port,
                                                                   const DnsOptions& options) {
    EventLoop loop;
    std::array<PendingQuery, 2> queries{};
    constexpr std::array<int, 2> kTypes{ARES_REC_TYPE_A, ARES_REC_TYPE_AAAA};
    const std::string name{host};
    int lastError = ARES_ENODATA;
    for (std::size_t i = 0; i < queries.size(); ++i) {
        queries[i].fd = android_res_nquery(NETWORK_UNSPECIFIED, name.c_str(), 1, kTypes[i], 0);
        if (queries[i].fd < 0) {
            lastError = queries[i].fd;
            continue;
        }
        if (auto watched = loop.watch(queries[i].fd, {true, false}, i); !watched) {
            return std::unexpected(DnsError{DnsError::Kind::IoFailed, watched.error()});
        }
    }
    std::stop_callback canceled{options.stop, [&] { (void)loop.wake(); }};
    std::vector<TcpAddress> addresses;
    while (loop.watchedCount() != 0) {
        if (options.stop.stop_requested()) {
            return std::unexpected(DnsError{DnsError::Kind::Canceled});
        }
        const auto remaining =
            std::chrono::ceil<std::chrono::milliseconds>(options.deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) {
            return std::unexpected(DnsError{DnsError::Kind::TimedOut});
        }
        auto ready = loop.wait(static_cast<int>(std::min<std::int64_t>(remaining, 3600000)));
        if (!ready) {
            return std::unexpected(DnsError{DnsError::Kind::IoFailed, ready.error()});
        }
        for (const auto& event : *ready) {
            auto& query = queries[static_cast<std::size_t>(event.token)];
            std::array<std::uint8_t, 65536> answer{};
            int rcode = 0;
            loop.unwatch(query.fd);
            const int length = android_res_nresult(query.fd, &rcode, answer.data(), answer.size());
            query.fd = -1; // nresult closes the descriptor, even on failure.
            if (length < 0 || rcode != 0) {
                lastError = length < 0 ? length : rcode;
                continue;
            }
            ares_dns_record_t* raw = nullptr;
            const auto parsed = ares_dns_parse(answer.data(), static_cast<std::size_t>(length), 0, &raw);
            const std::unique_ptr<ares_dns_record_t, decltype(&ares_dns_record_destroy)> record{
                raw, ares_dns_record_destroy};
            if (parsed != ARES_SUCCESS) {
                lastError = parsed;
                continue;
            }
            const std::size_t count = ares_dns_record_rr_cnt(record.get(), ARES_SECTION_ANSWER);
            for (std::size_t i = 0; i < count; ++i) {
                const auto* rr = ares_dns_record_rr_get_const(record.get(), ARES_SECTION_ANSWER, i);
                const auto type = ares_dns_rr_get_type(rr);
                const void* address = nullptr;
                int family = 0;
                if (type == ARES_REC_TYPE_A) {
                    address = ares_dns_rr_get_addr(rr, ARES_RR_A_ADDR);
                    family = AF_INET;
                } else if (type == ARES_REC_TYPE_AAAA) {
                    address = ares_dns_rr_get_addr6(rr, ARES_RR_AAAA_ADDR);
                    family = AF_INET6;
                }
                std::array<char, INET6_ADDRSTRLEN> text{};
                if (!address || !inet_ntop(family, address, text.data(), static_cast<socklen_t>(text.size()))) {
                    continue;
                }
                if (addresses.size() == 32) {
                    return std::unexpected(DnsError{DnsError::Kind::Limit});
                }
                if (auto endpoint = TcpAddress::literal(text.data(), port)) {
                    addresses.push_back(*endpoint);
                }
            }
        }
    }
    if (options.stop.stop_requested()) {
        return std::unexpected(DnsError{DnsError::Kind::Canceled});
    }
    if (std::chrono::steady_clock::now() >= options.deadline) {
        return std::unexpected(DnsError{DnsError::Kind::TimedOut});
    }
    if (addresses.empty()) {
        return std::unexpected(DnsError{DnsError::Kind::ResolveFailed, lastError});
    }
    return addresses;
}

} // namespace basalt::wire::io::detail
