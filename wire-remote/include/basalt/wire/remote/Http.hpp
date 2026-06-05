#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::remote::http {

struct Request {
    std::string method;
    std::string path;
    std::string query;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;

    /// Header names are stored lowercase.
    [[nodiscard]] std::optional<std::string_view> header(std::string_view lowerName) const;
};

enum class ParseResult : std::uint8_t { NeedMore, Request, Malformed, HeadTooLarge, BodyTooLarge, Unsupported };

inline constexpr std::size_t kMaxHeadBytes = 16384;

/// Consumes one complete request from the front of `buffer`.
[[nodiscard]] ParseResult take(std::string& buffer, Request& into, std::size_t maxBody);

[[nodiscard]] std::string_view reason(int status) noexcept;

/// Always closes or keeps the connection explicitly, so a client never has to guess.
[[nodiscard]] std::string response(int status, std::string_view contentType, std::string_view body, bool keepAlive,
                                   std::string_view extraHeaders = {});

[[nodiscard]] bool tokenListContains(std::string_view list, std::string_view wanted);

[[nodiscard]] std::string lower(std::string_view text);

/// `{"error":"<message>"}`.
[[nodiscard]] std::string errorBody(std::string_view message);

/// Length-independent in time for equal-length inputs; never short-circuits on the first difference.
[[nodiscard]] bool constantTimeEquals(std::string_view a, std::string_view b) noexcept;

} // namespace basalt::wire::remote::http
