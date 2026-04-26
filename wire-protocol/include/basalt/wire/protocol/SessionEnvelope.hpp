#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <variant>
#include <vector>

namespace basalt::wire::protocol::sessionEnvelope {

inline constexpr std::uint8_t kVersion1 = 1;

/// A data frame carries one original packet untouched; a control frame carries one of the messages below.
inline constexpr std::size_t kMaxDataBytes = 2 * 1024 * 1024;
inline constexpr std::size_t kMaxControlBytes = 512;

enum class Edition : std::uint8_t { Java = 0, Bedrock = 1 };

/// Last message in the old codec on the sender's side: every frame after it is an envelope frame.
struct Open {
    std::uint8_t version = kVersion1;
    Edition edition = Edition::Java;
    std::int32_t protocol = 0;

    friend bool operator==(const Open&, const Open&) = default;
};

/// The receiving side's agreement. Nothing but this and Close may follow Open until it arrives.
struct Opened {
    std::uint8_t version = kVersion1;

    friend bool operator==(const Opened&, const Opened&) = default;
};

enum class CloseReason : std::uint8_t { Normal = 0, Unsupported = 1, Malformed = 2, Superseded = 3 };

struct Close {
    CloseReason reason = CloseReason::Normal;

    friend bool operator==(const Close&, const Close&) = default;
};

using Control = std::variant<Open, Opened, Close>;

enum class DecodeError { kEmpty, kUnknownKind, kUnknownControl, kMalformed, kTooLarge };

struct Data {
    std::span<const std::uint8_t> packet;
};

using Frame = std::variant<Data, Control>;

[[nodiscard]] std::vector<std::uint8_t> encodeData(std::span<const std::uint8_t> packet);
[[nodiscard]] std::vector<std::uint8_t> encodeControl(const Control& control);

/// The returned data view points into `frame`.
[[nodiscard]] std::expected<Frame, DecodeError> decode(std::span<const std::uint8_t> frame);

} // namespace basalt::wire::protocol::sessionEnvelope
