#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace basalt::wire::bedrock::nethernet {

inline constexpr std::size_t kMaxHeaderBytes = 16 * 1024;
inline constexpr std::size_t kMaxBodyBytes = 1U << 20U;

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
};

enum class HttpParse : unsigned char { Incomplete, Complete, Bad, TooLarge };

/// Just enough HTTP/1.1 for the two signaling requests: a request line, a content length and a body.
[[nodiscard]] HttpParse parseHttpRequest(std::string_view buffer, HttpRequest& request);

[[nodiscard]] std::string httpResponse(int status, std::string_view contentType, std::string_view body);

} // namespace basalt::wire::bedrock::nethernet
