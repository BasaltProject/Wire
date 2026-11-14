#pragma once

#include <basalt/wire/multiversion/java/EntityDataIndexes.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace basalt::wire::multiversion::java {

enum class EntityDataExtraKind : std::uint8_t {
    /// `a`, sent as the native kind `b`: 0 byte, 1 int, 8 boolean.
    kConstant,
    kCopy,
    /// True when the source byte has bit `a`.
    kFlagFromBit,
    /// `a` when the source is true, ORed with `b`; replaces the source.
    kByteFromFlag,
    /// The same, also sent while the source is absent.
    kByteFromFlagAlways,
    /// The source number plus `a`; replaces the source.
    kOffset,
    /// `a` minus the source number; replaces the source.
    kReverse,
    /// True when the source holds anything but zero.
    kNonZero,
    /// The name the table `a` gives the source number; replaces the source.
    kName,
    /// The colour of the first effect particle in the source list, or 0; replaces the source.
    kEffectColour,
    /// The source byte with bit `b` set while the native boolean at `a` is true; replaces the source.
    kWithBit,
};

/// A field an older version has and the native one does not, placed in the older numbering of its link.
struct EntityDataExtra {
    std::span<const std::int32_t> types;
    std::int32_t index;
    EntityDataExtraKind kind;
    /// The native index it is derived from, or -1.
    std::int32_t source = -1;
    std::int32_t a = 0;
    std::int32_t b = 0;
    /// The oldest far end that has it.
    std::int32_t since = 0;
};

/// Fields `link` drops on top of its rules, in the same numbering as the rules and before them.
[[nodiscard]] std::span<const EntityDataIndexRule> entityDataExtraDrops(std::int32_t link);

/// Fields `link` places elsewhere after its rules, room left for what the older side adds.
[[nodiscard]] std::span<const EntityDataIndexMove> entityDataExtraSwaps(std::int32_t link);

[[nodiscard]] std::span<const EntityDataExtra> entityDataExtras(std::int32_t link);

[[nodiscard]] bool replacesSource(EntityDataExtraKind kind);

/// What `extra` holds for `farEnd` given the native fields of the packet.
[[nodiscard]] std::optional<protocol::play::SetEntityDataPacket::Value>
extraValue(const EntityDataExtra& extra, const protocol::play::SetEntityDataPacket& data, std::int32_t farEnd);

/// The profession a version before 1.14 numbers a native one as.
[[nodiscard]] std::int32_t legacyProfession(std::int32_t profession, std::int32_t farEnd);

} // namespace basalt::wire::multiversion::java
