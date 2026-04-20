#pragma once

#include <basalt/wire/bedrock/BedrockPacketIds.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::wire::bedrock {

/// What to do with a client whose Bedrock protocol is newer than anything this build speaks.
enum class BedrockFuturePolicy : std::uint8_t {
    Accept,
    Warn,
    Refuse,
};

[[nodiscard]] std::optional<BedrockFuturePolicy> parseBedrockFuturePolicy(std::string_view name);

/// The answer to "may this client in", and what it is told when the answer is no.
struct BedrockVerdict {
    enum class Kind : std::uint8_t {
        Speak,
        Unknown,
        TooOld,
        TooNew,
    };

    Kind kind = Kind::Speak;
    /// Only ever set for Unknown.
    bool warn = false;

    [[nodiscard]] bool allowed() const noexcept { return kind == Kind::Speak || kind == Kind::Unknown; }
};

/// Java keeps everything it ever spoke, because a player can choose an old version in the launcher.
class BedrockGate {
public:
    BedrockGate() = default;

    BedrockGate(std::set<std::int32_t> supported, BedrockFuturePolicy futureProtocols)
        : supported_(std::move(supported)), futureProtocols_(futureProtocols) {}

    [[nodiscard]] static std::set<std::int32_t> buildSupport();

    [[nodiscard]] const std::set<std::int32_t>& supported() const noexcept { return supported_; }

    [[nodiscard]] BedrockFuturePolicy futureProtocols() const noexcept { return futureProtocols_; }

    [[nodiscard]] BedrockVerdict verdict(std::int32_t protocol) const;

    /// What a refused client reads, which names the version it would need.
    [[nodiscard]] std::string refusalFor(const BedrockVerdict& verdict, std::string_view gameVersion) const;

private:
    std::set<std::int32_t> supported_{kNativeBedrockProtocol};
    BedrockFuturePolicy futureProtocols_ = BedrockFuturePolicy::Refuse;
};

} // namespace basalt::wire::bedrock
