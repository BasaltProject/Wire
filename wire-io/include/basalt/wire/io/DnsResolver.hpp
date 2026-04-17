#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <string_view>
#include <vector>

namespace basalt::wire::io {

class TcpAddress;

struct DnsError {
    enum class Kind { InvalidRequest, InitializeFailed, ResolveFailed, TimedOut, Canceled, Limit, IoFailed };
    Kind kind;
    int code = 0;
};

struct DnsOptions {
    std::chrono::steady_clock::time_point deadline;
    std::stop_token stop;
    /// Empty uses native resolver configuration; explicit numeric servers use c-ares CSV syntax.
    std::string_view servers;
};

/// Runs on a bounded worker, never an I/O owner. Network waits honor deadline/stop;
/// initial resolver configuration/hosts-file reads remain local OS filesystem operations.
[[nodiscard]] std::expected<std::vector<TcpAddress>, DnsError> resolveDns(std::string_view host, std::uint16_t port,
                                                                          const DnsOptions& options);

} // namespace basalt::wire::io
