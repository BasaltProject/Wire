#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::bedrock {

/// What the server needs from a client's SDP offer.
struct NetherNetOffer {
    std::string iceUfrag;
    std::string icePwd;
    std::string fingerprintAlgorithm;
    std::string fingerprintDigest;
    std::string setup;
    std::uint32_t maxMessageSize = 0;
    std::string identity;
};

struct NetherNetAnswer {
    std::string iceUfrag;
    std::string icePwd;
    std::string fingerprintDigest;
    std::string identity;
    std::string host;
    std::uint16_t port = 0;
};

[[nodiscard]] std::expected<NetherNetOffer, std::string> parseNetherNetOffer(std::string_view sdp);

[[nodiscard]] std::string buildNetherNetAnswer(const NetherNetAnswer& answer, std::uint64_t sessionId);

} // namespace basalt::wire::bedrock
