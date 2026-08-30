#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::wire::nbt {

/// The 13 NBT tag type ids, unchanged since Minecraft Beta 1.3.
enum class NbtType : std::uint8_t {
    End = 0,
    Byte = 1,
    Short = 2,
    Int = 3,
    Long = 4,
    Float = 5,
    Double = 6,
    ByteArray = 7,
    String = 8,
    List = 9,
    Compound = 10,
    IntArray = 11,
    LongArray = 12,
};

[[nodiscard]] constexpr std::optional<NbtType> nbtTypeFromId(std::uint8_t id) noexcept {
    if (id > static_cast<std::uint8_t>(NbtType::LongArray)) {
        return std::nullopt;
    }
    return static_cast<NbtType>(id);
}

[[nodiscard]] std::string_view nbtTypeName(NbtType type) noexcept;

/// Why binary or SNBT input did not describe a well-formed NBT document. Parsing NBT off the wire
/// is the server's main attack surface, so failure is a value the caller has to look at, never an
/// exception thrown out of a packet handler.
enum class NbtError : std::uint8_t {
    UnexpectedEnd,
    UnknownTagId,
    NegativeSize,
    StringTooLong,
    TruncatedUtf8,
    InvalidUtf8,
    MixedListTypes,
    DepthLimitExceeded,
    TrailingInput,
    SyntaxError,
};

[[nodiscard]] std::string_view nbtErrorMessage(NbtError error) noexcept;

} // namespace basalt::wire::nbt
