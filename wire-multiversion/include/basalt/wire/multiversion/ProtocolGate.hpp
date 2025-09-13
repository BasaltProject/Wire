#pragma once

#include <basalt/wire/multiversion/ProtocolVersions.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string_view>
#include <variant>

namespace basalt::wire::multiversion {

/// What to do with a client whose protocol this build does not know.
enum class UnknownProtocolPolicy : std::uint8_t {
    Accept,
    Warn,
    Refuse,
};

/// Nothing rather than a default, so a typo in the file is reported instead of guessed.
[[nodiscard]] std::optional<UnknownProtocolPolicy> parseUnknownProtocolPolicy(std::string_view name);

/// The answer to "may this client in", and why.
///
/// Translated and Unknown both let someone in, and the difference matters to whoever reads the log:
/// one is a version this build was built to speak, the other is a bet.
struct ProtocolVerdict {
    /// Speaks what the server speaks. Nothing is translated.
    struct Native {
        friend bool operator==(const Native&, const Native&) = default;
    };

    struct Translated {
        ProtocolVersion version;

        friend bool operator==(const Translated&, const Translated&) = default;
    };

    /// Let in without a translator: newer than this build, or a snapshot.
    struct Unknown {
        std::int32_t protocol = 0;
        bool warn = false;

        friend bool operator==(const Unknown&, const Unknown&) = default;
    };

    /// Kicked at the handshake, with what the player reads.
    struct Refused {
        enum class Reason : std::uint8_t {
            TooOld,
            TooNew,
            Snapshot,
            NotTranslated,
        };

        std::int32_t protocol = 0;
        Reason reason = Reason::TooOld;

        friend bool operator==(const Refused&, const Refused&) = default;
    };

    std::variant<Native, Translated, Unknown, Refused> value;

    friend bool operator==(const ProtocolVerdict&, const ProtocolVerdict&) = default;
};

/// The door. It knows which protocols this build can translate and what the operator decided about
/// the ones it cannot, and it answers before a player exists.
///
/// It deliberately has no idea how translation works: the supported set is a set of numbers handed
/// to it, so the day a link is added the gate changes by itself.
class ProtocolGate {
public:
    ProtocolGate(std::set<std::int32_t> supported, UnknownProtocolPolicy futurePolicy,
                 UnknownProtocolPolicy snapshotPolicy, std::int32_t native = protocolVersions::kNative)
        : supported_(std::move(supported)), futurePolicy_(futurePolicy), snapshotPolicy_(snapshotPolicy),
          native_(native) {}

    [[nodiscard]] const std::set<std::int32_t>& supported() const noexcept { return supported_; }

    [[nodiscard]] ProtocolVerdict verdictFor(std::int32_t protocol) const;

private:
    std::set<std::int32_t> supported_;
    UnknownProtocolPolicy futurePolicy_;
    UnknownProtocolPolicy snapshotPolicy_;
    std::int32_t native_;
};

} // namespace basalt::wire::multiversion
