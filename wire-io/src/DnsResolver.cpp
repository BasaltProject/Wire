#include <basalt/wire/io/DnsResolver.hpp>
#include <basalt/wire/io/EventLoop.hpp>
#include <basalt/wire/io/TcpSocket.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <algorithm>
#include <ares.h>
#include <array>
#include <mutex>
#include <optional>
#include <string>

namespace basalt::wire::io {
namespace {
using Clock = std::chrono::steady_clock;
#ifdef _WIN32
using AddressTextLength = std::size_t;
#else
using AddressTextLength = socklen_t;
#endif
constexpr std::size_t kMaxAddresses = 32;

struct Query {
    EventLoop loop;
    ares_channel_t* channel = nullptr;
    std::vector<TcpAddress> addresses;
    std::optional<DnsError> error;
    std::uint16_t port;
    bool done = false;

    explicit Query(std::uint16_t requestedPort) : port(requestedPort) { addresses.reserve(kMaxAddresses); }

    ~Query() {
        if (channel)
            ares_destroy(channel);
    }

    static void socketState(void* context, ares_socket_t fd, int readable, int writable) noexcept {
        auto& query = *static_cast<Query*>(context);
        const auto handle = static_cast<std::int64_t>(fd);
        try {
            if (!readable && !writable) {
                query.loop.unwatch(handle);
            } else if (auto result = query.loop.watch(handle, {readable != 0, writable != 0}, 0); !result) {
                query.error = DnsError{DnsError::Kind::IoFailed, result.error()};
            }
        } catch (...) {
            query.error = DnsError{DnsError::Kind::Limit, ARES_ENOMEM};
        }
    }

    static void complete(void* context, int status, int, ares_addrinfo* result) noexcept {
        auto& query = *static_cast<Query*>(context);
        query.done = true;
        if (status != ARES_SUCCESS) {
            query.error = DnsError{DnsError::Kind::ResolveFailed, status};
        } else
            try {
                for (auto* node = result->nodes; node; node = node->ai_next) {
                    std::array<char, INET6_ADDRSTRLEN> text{};
                    const void* address = nullptr;
                    if (node->ai_family == AF_INET &&
                        static_cast<std::size_t>(node->ai_addrlen) >= sizeof(sockaddr_in)) {
                        address = &reinterpret_cast<const sockaddr_in*>(node->ai_addr)->sin_addr;
                    } else if (node->ai_family == AF_INET6 &&
                               static_cast<std::size_t>(node->ai_addrlen) >= sizeof(sockaddr_in6)) {
                        address = &reinterpret_cast<const sockaddr_in6*>(node->ai_addr)->sin6_addr;
                    } else {
                        continue;
                    }
                    if (!inet_ntop(node->ai_family, address, text.data(), static_cast<AddressTextLength>(text.size())))
                        continue;
                    if (query.addresses.size() == kMaxAddresses) {
                        query.error = DnsError{DnsError::Kind::Limit};
                        break;
                    }
                    std::string literal{text.data()};
                    if (node->ai_family == AF_INET6) {
                        const auto scope = reinterpret_cast<const sockaddr_in6*>(node->ai_addr)->sin6_scope_id;
                        if (scope != 0)
                            literal += "%" + std::to_string(scope);
                    }
                    auto endpoint = TcpAddress::literal(literal, query.port);
                    if (endpoint)
                        query.addresses.push_back(*endpoint);
                }
                if (query.addresses.empty() && !query.error)
                    query.error = DnsError{DnsError::Kind::ResolveFailed, ARES_ENODATA};
            } catch (...) {
                query.error = DnsError{DnsError::Kind::Limit, ARES_ENOMEM};
            }
        if (result)
            ares_freeaddrinfo(result);
    }
};

std::optional<DnsError> interruption(const DnsOptions& options) {
    if (options.stop.stop_requested())
        return DnsError{DnsError::Kind::Canceled};
    if (Clock::now() >= options.deadline)
        return DnsError{DnsError::Kind::TimedOut};
    return std::nullopt;
}
} // namespace

std::expected<std::vector<TcpAddress>, DnsError> resolveDns(std::string_view host, std::uint16_t port,
                                                            const DnsOptions& options) {
    if (auto stopped = interruption(options))
        return std::unexpected(*stopped);
    if (host.empty() || host.size() > 253 || port == 0 || options.servers.size() > 4096 ||
        host.find('\0') != std::string_view::npos || options.servers.find('\0') != std::string_view::npos)
        return std::unexpected(DnsError{DnsError::Kind::InvalidRequest});
    for (char character : host) {
        const auto c = static_cast<unsigned char>(character);
        if (c <= 32 || c == 127)
            return std::unexpected(DnsError{DnsError::Kind::InvalidRequest});
    }
    if (auto address = TcpAddress::literal(host, port))
        return std::vector<TcpAddress>{*address};

    // Process lifetime: global teardown must not race other resolver channels or Winsock users.
    static std::once_flag initialized;
    static int initializationStatus = ARES_ENOTINITIALIZED;
    std::call_once(initialized, [] { initializationStatus = ares_library_init(ARES_LIB_INIT_ALL); });
    if (initializationStatus != ARES_SUCCESS)
        return std::unexpected(DnsError{DnsError::Kind::InitializeFailed, initializationStatus});

    Query query{port};
    ares_options setup{};
    setup.sock_state_cb = Query::socketState;
    setup.sock_state_cb_data = &query;
    setup.tries = 2;
    setup.timeout = 1000;
    const int status =
        ares_init_options(&query.channel, &setup, ARES_OPT_SOCK_STATE_CB | ARES_OPT_TRIES | ARES_OPT_TIMEOUTMS);
    if (status != ARES_SUCCESS)
        return std::unexpected(DnsError{DnsError::Kind::InitializeFailed, status});
    if (!options.servers.empty()) {
        const std::string servers{options.servers};
        const int serverStatus = ares_set_servers_ports_csv(query.channel, servers.c_str());
        if (serverStatus != ARES_SUCCESS)
            return std::unexpected(DnsError{DnsError::Kind::InvalidRequest, serverStatus});
    }
    if (auto stopped = interruption(options))
        return std::unexpected(*stopped);

    std::stop_callback canceled{options.stop, [&] { (void)query.loop.wake(); }};
    const std::string name{host};
    ares_addrinfo_hints hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = ARES_AI_NOSORT;
    ares_getaddrinfo(query.channel, name.c_str(), nullptr, &hints, Query::complete, &query);
    while (!query.done && !query.error) {
        if (auto stopped = interruption(options))
            return std::unexpected(*stopped);
        const auto remaining = std::max<std::int64_t>(
            0, std::chrono::ceil<std::chrono::milliseconds>(options.deadline - Clock::now()).count());
        timeval maximum{};
        maximum.tv_sec = static_cast<decltype(maximum.tv_sec)>(std::min<std::int64_t>(remaining / 1000, 3600));
        maximum.tv_usec = static_cast<decltype(maximum.tv_usec)>((remaining % 1000) * 1000);
        timeval requested{};
        const auto* timeout = ares_timeout(query.channel, &maximum, &requested);
        const auto milliseconds = static_cast<std::int64_t>(timeout->tv_sec) * 1000 + (timeout->tv_usec + 999) / 1000;
        auto ready = query.loop.wait(static_cast<int>(std::clamp<std::int64_t>(milliseconds, 0, 3600000)));
        if (!ready)
            return std::unexpected(DnsError{DnsError::Kind::IoFailed, ready.error()});
        std::array<ares_fd_events_t, 64> events{};
        if (ready->size() > events.size())
            return std::unexpected(DnsError{DnsError::Kind::Limit});
        std::size_t count = 0;
        for (const auto& item : *ready) {
            auto& event = events[count++];
            event.fd = static_cast<ares_socket_t>(item.handle);
            event.events = (item.readable || item.closed ? ARES_FD_EVENT_READ : 0) |
                           (item.writable || item.closed ? ARES_FD_EVENT_WRITE : 0);
        }
        if (auto stopped = interruption(options))
            return std::unexpected(*stopped);
        const auto processed = ares_process_fds(query.channel, events.data(), count, ARES_PROCESS_FLAG_NONE);
        if (processed != ARES_SUCCESS)
            return std::unexpected(DnsError{DnsError::Kind::ResolveFailed, processed});
    }
    if (auto stopped = interruption(options))
        return std::unexpected(*stopped);
    if (query.error)
        return std::unexpected(*query.error);
    return std::move(query.addresses);
}

} // namespace basalt::wire::io
