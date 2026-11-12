#include "NetherNetHttp.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace basalt::wire::bedrock::nethernet {
namespace {

std::string lower(std::string_view text) {
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string_view reason(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 413:
        return "Payload Too Large";
    case 503:
        return "Service Unavailable";
    default:
        return "Internal Server Error";
    }
}

} // namespace

HttpParse parseHttpRequest(std::string_view buffer, HttpRequest& request) {
    const std::size_t headerEnd = buffer.find("\r\n\r\n");
    if (headerEnd == std::string_view::npos) {
        return buffer.size() > kMaxHeaderBytes ? HttpParse::TooLarge : HttpParse::Incomplete;
    }
    const std::string_view head = buffer.substr(0, headerEnd);

    const std::size_t lineEnd = head.find("\r\n");
    const std::string_view requestLine = head.substr(0, lineEnd);
    const std::size_t firstSpace = requestLine.find(' ');
    const std::size_t secondSpace = requestLine.rfind(' ');
    if (firstSpace == std::string_view::npos || secondSpace == firstSpace) {
        return HttpParse::Bad;
    }
    request.method = std::string{requestLine.substr(0, firstSpace)};
    request.path = std::string{requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1)};

    std::size_t contentLength = 0;
    std::size_t position = lineEnd == std::string_view::npos ? head.size() : lineEnd + 2;
    while (position < head.size()) {
        const std::size_t end = head.find("\r\n", position);
        const std::string_view line = head.substr(position, end == std::string_view::npos ? end : end - position);
        position = end == std::string_view::npos ? head.size() : end + 2;
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos || lower(line.substr(0, colon)) != "content-length") {
            continue;
        }
        std::string_view value = line.substr(colon + 1);
        while (!value.empty() && value.front() == ' ') {
            value.remove_prefix(1);
        }
        const auto [pointer, error] = std::from_chars(value.data(), value.data() + value.size(), contentLength);
        if (error != std::errc{}) {
            return HttpParse::Bad;
        }
    }
    if (contentLength > kMaxBodyBytes) {
        return HttpParse::TooLarge;
    }

    const std::size_t bodyStart = headerEnd + 4;
    if (buffer.size() < bodyStart + contentLength) {
        return HttpParse::Incomplete;
    }
    request.body = std::string{buffer.substr(bodyStart, contentLength)};
    return HttpParse::Complete;
}

std::string httpResponse(int status, std::string_view contentType, std::string_view body) {
    std::string out = "HTTP/1.1 " + std::to_string(status) + " " + std::string{reason(status)} + "\r\n";
    out += "Content-Type: " + std::string{contentType} + "\r\n";
    out += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    out += "Connection: close\r\n\r\n";
    out += body;
    return out;
}

} // namespace basalt::wire::bedrock::nethernet
