#include <basalt/wire/bedrock/BedrockGate.hpp>

#include <algorithm>
#include <iterator>

namespace basalt::wire::bedrock {

std::optional<BedrockFuturePolicy> parseBedrockFuturePolicy(const std::string_view name) {
    if (name == "accept") {
        return BedrockFuturePolicy::Accept;
    }
    if (name == "warn") {
        return BedrockFuturePolicy::Warn;
    }
    if (name == "refuse") {
        return BedrockFuturePolicy::Refuse;
    }
    return std::nullopt;
}

std::set<std::int32_t> BedrockGate::buildSupport() {
    return {std::begin(kBedrockProtocols), std::end(kBedrockProtocols)};
}

BedrockVerdict BedrockGate::verdict(const std::int32_t protocol) const {
    if (supported_.contains(protocol)) {
        return BedrockVerdict{BedrockVerdict::Kind::Speak, false};
    }
    if (supported_.empty() || protocol < *supported_.rbegin()) {
        return BedrockVerdict{BedrockVerdict::Kind::TooOld, false};
    }
    switch (futureProtocols_) {
    case BedrockFuturePolicy::Accept:
        return BedrockVerdict{BedrockVerdict::Kind::Unknown, false};
    case BedrockFuturePolicy::Warn:
        return BedrockVerdict{BedrockVerdict::Kind::Unknown, true};
    case BedrockFuturePolicy::Refuse:
        break;
    }
    return BedrockVerdict{BedrockVerdict::Kind::TooNew, false};
}

std::string BedrockGate::refusalFor(const BedrockVerdict& verdict, const std::string_view gameVersion) const {
    switch (verdict.kind) {
    case BedrockVerdict::Kind::TooOld:
        return "This server runs Minecraft " + std::string{gameVersion} + ". Update your game and come back.";
    case BedrockVerdict::Kind::TooNew:
        return "This server runs Minecraft " + std::string{gameVersion} + " and does not speak your version yet.";
    default:
        return {};
    }
}

} // namespace basalt::wire::bedrock
