#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace basalt::wire::io::proxyProtocol {

enum class Status : std::uint8_t { kNeedMore, kInvalid, kDone };

struct Result {
    Status status = Status::kNeedMore;
    std::size_t consumed = 0;
    /// Empty for a LOCAL or UNKNOWN header, which carries no client address.
    std::string address;
    std::uint16_t port = 0;
    /// Bytes the header needs in all, as far as is known so far, while the status is kNeedMore.
    std::size_t needed = 0;
};

/// Reads one HAProxy header (v1 or v2) from the start of `bytes` without waiting for more than it needs.
[[nodiscard]] Result parse(std::span<const std::uint8_t> bytes);

} // namespace basalt::wire::io::proxyProtocol
