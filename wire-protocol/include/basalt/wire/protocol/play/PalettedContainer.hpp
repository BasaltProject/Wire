#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/BitStorage.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::protocol::play::palettedContainer {

/// Which one applies comes from the section's distinct value count, not from the caller.
inline constexpr int kBlockMinIndirectBits = 4;
inline constexpr int kBlockMaxIndirectBits = 8;
inline constexpr int kBiomeMinIndirectBits = 0;
inline constexpr int kBiomeMaxIndirectBits = 3;

void write(io::ByteBuffer& buffer, std::span<const std::int32_t> values, int minIndirectBits, int maxIndirectBits,
           int directBits);

void writePacked(io::ByteBuffer& buffer, const BitStorageView& storage, std::span<const std::int32_t> palette,
                 int directBits);

[[nodiscard]] std::vector<std::int32_t> read(io::ByteBuffer& buffer, std::size_t size, int maxIndirectBits);

} // namespace basalt::wire::protocol::play::palettedContainer
