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

struct JavaSignText {
    std::string front;
    std::string back;
    bool waxed = false;
};

[[nodiscard]] std::optional<JavaSignText> readJavaSign(const nbt::NbtCompound& data);

/// Appends the Bedrock tag for a Java block entity and says whether it knew the type.
bool writeJavaBlockEntity(io::ByteBuffer& tags, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t javaType,
                          const nbt::NbtCompound& data);

} // namespace basalt::wire::bedrock
