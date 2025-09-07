#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/BitStorage.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::protocol::play::palettedContainer {

/// The block and biome section codec vanilla's PalettedContainer uses inside chunk data:
/// single-valued (zero bits, one palette entry, no data words), indirect (a small local palette
/// plus packed indices into it), or direct (no palette, the registry ids packed as they are). Which
/// one applies comes from the section's distinct value count, not from the caller.
inline constexpr int kBlockMinIndirectBits = 4;
inline constexpr int kBlockMaxIndirectBits = 8;
inline constexpr int kBiomeMinIndirectBits = 0;
inline constexpr int kBiomeMaxIndirectBits = 3;

void write(io::ByteBuffer& buffer, std::span<const std::int32_t> values, int minIndirectBits, int maxIndirectBits,
           int directBits);

/// Writes a section that is already packed, straight from its own words, with the palette it was
/// packed against. Nothing is expanded and nothing is re-packed: this is the path a chunk packet
/// takes.
void writePacked(io::ByteBuffer& buffer, const BitStorageView& storage, std::span<const std::int32_t> palette,
                 int directBits);

/// Reads a section back into one value per entry.
[[nodiscard]] std::vector<std::int32_t> read(io::ByteBuffer& buffer, std::size_t size, int maxIndirectBits);

} // namespace basalt::wire::protocol::play::palettedContainer
