#include <basalt/wire/bedrock/NetherNetSdp.hpp>

#include <charconv>

namespace basalt::wire::bedrock {
namespace {

constexpr std::string_view kNewline = "\r\n";

std::string_view trimLine(std::string_view line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
        line.remove_suffix(1);
    }
    return line;
}

} // namespace

std::expected<NetherNetOffer, std::string> parseNetherNetOffer(const std::string_view sdp) {
    NetherNetOffer offer;
    bool sawMedia = false;
    std::size_t position = 0;
    while (position < sdp.size()) {
        const std::size_t end = sdp.find('\n', position);
        const std::string_view line =
            trimLine(sdp.substr(position, end == std::string_view::npos ? std::string_view::npos : end - position));
        position = end == std::string_view::npos ? sdp.size() : end + 1;

        if (line.starts_with("m=")) {
            if (sawMedia) {
                return std::unexpected{"more than one media section"};
            }
            sawMedia = true;
            continue;
        }
        if (!line.starts_with("a=")) {
            continue;
        }
        const std::string_view attribute = line.substr(2);
        const std::size_t colon = attribute.find(':');
        const std::string_view key = attribute.substr(0, colon);
        const std::string_view value =
            colon == std::string_view::npos ? std::string_view{} : attribute.substr(colon + 1);

        if (key == "ice-ufrag") {
            offer.iceUfrag = value;
        } else if (key == "ice-pwd") {
            offer.icePwd = value;
        } else if (key == "fingerprint") {
            const std::size_t space = value.find(' ');
            if (space == std::string_view::npos) {
                return std::unexpected{"malformed fingerprint"};
            }
            offer.fingerprintAlgorithm = value.substr(0, space);
            offer.fingerprintDigest = value.substr(space + 1);
        } else if (key == "setup") {
            offer.setup = value;
        } else if (key == "identity") {
            offer.identity = value;
        } else if (key == "max-message-size") {
            std::uint32_t parsed = 0;
            const auto [pointer, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (error != std::errc{} || pointer != value.data() + value.size()) {
                return std::unexpected{"malformed max-message-size"};
            }
            offer.maxMessageSize = parsed;
        }
    }

    if (!sawMedia) {
        return std::unexpected{"no media section"};
    }
    if (offer.iceUfrag.empty() || offer.icePwd.empty()) {
        return std::unexpected{"missing ICE credentials"};
    }
    if (offer.fingerprintDigest.empty() || offer.fingerprintAlgorithm != "sha-256") {
        return std::unexpected{"missing sha-256 fingerprint"};
    }
    if (offer.setup != "actpass" && offer.setup != "passive") {
        return std::unexpected{"the offer must let the server be the DTLS client"};
    }
    return offer;
}

std::string buildNetherNetAnswer(const NetherNetAnswer& answer, const std::uint64_t sessionId) {
    std::string out;
    const auto line = [&out](const std::string_view text) {
        out += text;
        out += kNewline;
    };

    line("v=0");
    line("o=- " + std::to_string(sessionId) + " 2 IN IP4 127.0.0.1");
    line("s=-");
    line("t=0 0");
    line("a=group:BUNDLE 0");
    line("a=extmap-allow-mixed");
    line("a=msid-semantic: WMS");
    if (!answer.identity.empty()) {
        line("a=identity:" + answer.identity);
    }
    line("m=application " + std::to_string(answer.port) + " UDP/DTLS/SCTP webrtc-datachannel");
    line("c=IN IP4 " + answer.host);
    line("a=candidate:1 1 udp 2130706431 " + answer.host + " " + std::to_string(answer.port) + " typ host");
    line("a=ice-ufrag:" + answer.iceUfrag);
    line("a=ice-pwd:" + answer.icePwd);
    line("a=ice-options:trickle");
    line("a=fingerprint:sha-256 " + answer.fingerprintDigest);
    line("a=setup:active");
    line("a=mid:0");
    line("a=sctp-port:5000");
    line("a=max-message-size:262144");
    line("a=end-of-candidates");
    return out;
}

} // namespace basalt::wire::bedrock
