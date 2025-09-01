#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::nbt {

/// A root tag together with the name it was stored under, usually empty in modern usage.
struct NamedTag {
    std::string name;
    NbtTag tag;
};

/// A tag read without a name field, plus how many bytes of the input it consumed.
struct AnonymousTag {
    NbtTag tag;
    std::size_t bytesConsumed = 0;
};

/// Binary NBT reader and writer. Deliberately has no notion of gzip or zlib: compression is a
/// transport concern that callers with a compressor layer on top, which keeps wire-nbt free of
/// dependencies.
namespace nbtIO {

/// Guards against a crafted document nesting lists and compounds deep enough to run the stack out.
inline constexpr int kMaxDepth = 512;

[[nodiscard]] std::vector<std::uint8_t> write(const NbtTag& root, std::string_view name = "");

[[nodiscard]] std::expected<NamedTag, NbtError> read(std::span<const std::uint8_t> bytes);

/// The root tag carries no name, which is the shape modern network packets use.
[[nodiscard]] std::vector<std::uint8_t> writeAnonymous(const NbtTag& root);

[[nodiscard]] std::expected<AnonymousTag, NbtError> readAnonymous(std::span<const std::uint8_t> bytes);

} // namespace nbtIO

} // namespace basalt::wire::nbt
