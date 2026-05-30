#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::wire::bedrock {

/// Ids in the block entity type registry of 26.3.
inline constexpr std::int32_t kJavaSignType = 7;
inline constexpr std::int32_t kJavaHangingSignType = 8;
inline constexpr std::int32_t kJavaBannerType = 20;

struct JavaSignText {
    std::string front;
    std::string back;
    bool waxed = false;
};

[[nodiscard]] std::optional<JavaSignText> readJavaSign(const nbt::NbtCompound& data);

/// The dye of a bed or banner block state of 26.3, in dye order from white.
[[nodiscard]] std::optional<std::uint8_t> javaBedColour(std::int32_t state);
[[nodiscard]] std::optional<std::uint8_t> javaBannerColour(std::int32_t state);

/// Appends the Bedrock tag of a Java block entity (a banner needs the state under it); false if the type is unknown.
bool writeJavaBlockEntity(io::ByteBuffer& tags, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t javaType,
                          const nbt::NbtCompound& data, std::int32_t state);

} // namespace basalt::wire::bedrock
