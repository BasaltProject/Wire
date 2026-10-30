#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/remote/Http.hpp>

#include <algorithm>
#include <charconv>

namespace basalt::wire::remote::http {
namespace {

[[nodiscard]] std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

} // namespace

std::string lower(std::string_view text) {
    std::string out{text};
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

std::string errorBody(std::string_view message) {
    common::JsonObject object;
    object.set("error", common::JsonValue::of(std::string{message}));
    return common::writeJson(common::JsonValue::ofObject(std::move(object)));
}

bool constantTimeEquals(std::string_view a, std::string_view b) noexcept {
    unsigned char diff = a.size() == b.size() ? 0 : 1;
    const std::size_t length = std::max(a.size(), b.size());
    for (std::size_t i = 0; i < length; ++i) {
        const auto left = static_cast<unsigned char>(i < a.size() ? a[i] : 0);
        const auto right = static_cast<unsigned char>(i < b.size() ? b[i] : 0);
        diff = static_cast<unsigned char>(diff | (left ^ right));
    }
    return diff == 0;
}

std::optional<std::string_view> Request::header(std::string_view lowerName) const {
    for (const auto& [name, value] : headers) {
        if (name == lowerName) {
            return std::string_view{value};
        }
    }
    return std::nullopt;
}

bool tokenListContains(std::string_view list, std::string_view wanted) {
    while (!list.empty()) {
        const std::size_t comma = list.find(',');
        const std::string_view item = trim(list.substr(0, comma));
        if (lower(item) == wanted) {
            return true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        list.remove_prefix(comma + 1);
    }
    return false;
}

ParseResult take(std::string& buffer, Request& into, std::size_t maxBody) {
    const std::size_t headEnd = buffer.find("\r\n\r\n");
    if (headEnd == std::string::npos) {
        return buffer.size() > kMaxHeadBytes ? ParseResult::kHeadTooLarge : ParseResult::kNeedMore;
    }
    if (headEnd > kMaxHeadBytes) {
        return ParseResult::kHeadTooLarge;
    }
    const std::string_view head{buffer.data(), headEnd};
    const std::size_t lineEnd = head.find("\r\n");
    const std::string_view requestLine = head.substr(0, lineEnd);
    const std::size_t firstSpace = requestLine.find(' ');
    const std::size_t secondSpace = requestLine.rfind(' ');
    if (firstSpace == std::string_view::npos || secondSpace == firstSpace) {
        return ParseResult::kMalformed;
    }
    if (!requestLine.substr(secondSpace + 1).starts_with("HTTP/1.")) {
        return ParseResult::kMalformed;
    }
    Request request;
    request.method = std::string{requestLine.substr(0, firstSpace)};
    const std::string_view target = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    const std::size_t question = target.find('?');
    request.path = std::string{target.substr(0, question)};
    if (question != std::string_view::npos) {
        request.query = std::string{target.substr(question + 1)};
    }
    if (request.path.empty() || request.path.front() != '/') {
        return ParseResult::kMalformed;
    }
    std::string_view rest = lineEnd == std::string_view::npos ? std::string_view{} : head.substr(lineEnd + 2);
    while (!rest.empty()) {
        const std::size_t end = rest.find("\r\n");
        const std::string_view line = rest.substr(0, end);
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            return ParseResult::kMalformed;
        }
        request.headers.emplace_back(lower(trim(line.substr(0, colon))), std::string{trim(line.substr(colon + 1))});
        if (end == std::string_view::npos) {
            break;
        }
        rest.remove_prefix(end + 2);
    }
    if (request.header("transfer-encoding").has_value()) {
        return ParseResult::kUnsupported;
    }
    std::size_t length = 0;
    if (std::ranges::count(request.headers, "content-length", &std::pair<std::string, std::string>::first) > 1) {
        return ParseResult::kMalformed;
    }
    if (const auto written = request.header("content-length"); written.has_value()) {
        const auto parsed = std::from_chars(written->data(), written->data() + written->size(), length);
        if (parsed.ec != std::errc{} || parsed.ptr != written->data() + written->size()) {
            return ParseResult::kMalformed;
        }
        if (length > maxBody) {
            return ParseResult::kBodyTooLarge;
        }
    }
    const std::size_t total = headEnd + 4 + length;
    if (buffer.size() < total) {
        return ParseResult::kNeedMore;
    }
    request.body.assign(buffer, headEnd + 4, length);
    buffer.erase(0, total);
    into = std::move(request);
    return ParseResult::kRequest;
}

std::string_view reason(int status) noexcept {
    switch (status) {
    case 101:
        return "Switching Protocols";
    case 200:
        return "OK";
    case 202:
        return "Accepted";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 409:
        return "Conflict";
    case 413:
        return "Payload Too Large";
    case 422:
        return "Unprocessable Content";
    case 426:
        return "Upgrade Required";
    case 429:
        return "Too Many Requests";
    case 431:
        return "Request Header Fields Too Large";
    case 501:
        return "Not Implemented";
    case 503:
        return "Service Unavailable";
    default:
        return "Error";
    }
}

std::string response(int status, std::string_view contentType, std::string_view body, bool keepAlive,
                     std::string_view extraHeaders) {
    std::string out = "HTTP/1.1 " + std::to_string(status) + " " + std::string{reason(status)} + "\r\n";
    out += "Content-Type: " + std::string{contentType} + "\r\n";
    out += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    out += "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n";
    out += keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
    out += extraHeaders;
    out += "\r\n";
    out += body;
    return out;
}

} // namespace basalt::wire::remote::http
