#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::nbt {

struct NamedTag {
    std::string name;
    NbtTag tag;
};

struct AnonymousTag {
    NbtTag tag;
    std::size_t bytesConsumed = 0;
};

namespace nbtIO {

inline constexpr int kMaxDepth = 512;

[[nodiscard]] std::vector<std::uint8_t> write(const NbtTag& root, std::string_view name = "");

[[nodiscard]] std::expected<NamedTag, NbtError> read(std::span<const std::uint8_t> bytes);

[[nodiscard]] std::vector<std::uint8_t> writeAnonymous(const NbtTag& root);

[[nodiscard]] std::expected<AnonymousTag, NbtError> readAnonymous(std::span<const std::uint8_t> bytes);

} // namespace nbtIO

} // namespace basalt::wire::nbt
