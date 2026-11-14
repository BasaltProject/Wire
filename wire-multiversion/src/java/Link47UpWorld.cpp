#include <basalt/wire/multiversion/CompactBitStorage.hpp>
#include <basalt/wire/multiversion/java/Link47Up.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace pv = protocol;

constexpr std::int32_t kChunk19 = 0x20;
constexpr std::int32_t kUnloadChunk19 = 0x1D;
constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr std::size_t kBlockBytes = kBlocksPerSection * 2;
constexpr std::size_t kLightBytes = 2048;
constexpr std::size_t kBiomeBytes = 256;
constexpr int kMinBits = 4;
constexpr int kMaxIndirectBits = 8;
constexpr int kDirectBits = 13;
constexpr std::int32_t kLegacyStates = 4096;

/// A 1.8 section's little-endian `id << 4 | meta` shorts, as a 1.9 paletted container.
void writeBlocks(io::ByteBuffer& out, std::span<const std::uint8_t> shorts) {
    std::vector<std::int32_t> states(kBlocksPerSection);
    std::vector<std::int32_t> slot(kLegacyStates, -1);
    std::vector<std::int32_t> palette;
    std::vector<std::int32_t> indexes(kBlocksPerSection);
    for (std::size_t cell = 0; cell < kBlocksPerSection; ++cell) {
        const std::int32_t state = (shorts[cell * 2] | (shorts[cell * 2 + 1] << 8)) & (kLegacyStates - 1);
        states[cell] = state;
        std::int32_t& index = slot[static_cast<std::size_t>(state)];
        if (index < 0) {
            index = static_cast<std::int32_t>(palette.size());
            palette.push_back(state);
        }
        indexes[cell] = index;
    }
    int bits = std::max(kMinBits, static_cast<int>(std::bit_width(palette.size() - 1)));
    const bool direct = bits > kMaxIndirectBits;
    bits = direct ? kDirectBits : bits;
    out.writeByte(static_cast<std::uint8_t>(bits));
    pv::varInt::write(out, direct ? 0 : static_cast<std::int32_t>(palette.size()));
    if (!direct) {
        for (const std::int32_t state : palette) {
            pv::varInt::write(out, state);
        }
    }
    const std::vector<std::int64_t> words = compactBitStorage::pack(direct ? states : indexes, bits);
    pv::varInt::write(out, static_cast<std::int32_t>(words.size()));
    for (const std::int64_t word : words) {
        out.writeLong(word);
    }
}

/// One column in the 1.8 layout (blocks of every section, then their light, then biomes), in the 1.9 one.
[[nodiscard]] bool convertColumn(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t mask, bool full, bool sky) {
    const auto count = static_cast<std::size_t>(std::popcount(static_cast<std::uint32_t>(mask & 0xFFFF)));
    const std::span<const std::uint8_t> blocks = in.readSpan(count * kBlockBytes);
    const std::span<const std::uint8_t> blockLight = in.readSpan(count * kLightBytes);
    const std::span<const std::uint8_t> skyLight =
        sky ? in.readSpan(count * kLightBytes) : std::span<const std::uint8_t>{};
    const std::span<const std::uint8_t> biomes = full ? in.readSpan(kBiomeBytes) : std::span<const std::uint8_t>{};
    if (in.failed()) {
        return false;
    }
    for (std::size_t section = 0; section < count; ++section) {
        writeBlocks(out, blocks.subspan(section * kBlockBytes, kBlockBytes));
        out.writeBytes(blockLight.subspan(section * kLightBytes, kLightBytes));
        if (sky) {
            out.writeBytes(skyLight.subspan(section * kLightBytes, kLightBytes));
        }
    }
    out.writeBytes(biomes);
    return true;
}

[[nodiscard]] std::vector<std::uint8_t> chunkBody(std::int32_t x, std::int32_t z, bool full, std::int32_t mask,
                                                  const io::ByteBuffer& data) {
    io::ByteBuffer out;
    out.writeInt(x);
    out.writeInt(z);
    pv::writeBoolean(out, full);
    pv::varInt::write(out, mask);
    pv::varInt::write(out, static_cast<std::int32_t>(data.readableBytes()));
    out.writeBytes(data.readable());
    return out.toVector();
}

[[nodiscard]] std::vector<std::uint8_t> unloadBody(std::int32_t x, std::int32_t z) {
    io::ByteBuffer out;
    out.writeInt(x);
    out.writeInt(z);
    return out.toVector();
}

} // namespace

bool Link47Up::chunk(MutablePacket& packet) const {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = in.readInt();
    const std::int32_t z = in.readInt();
    const bool full = pv::readBoolean(in);
    const std::int32_t mask = static_cast<std::uint16_t>(in.readShort());
    const std::int32_t length = pv::varInt::read(in);
    if (in.failed() || length < 0) {
        return false;
    }
    if (full && mask == 0) {
        packet.body = unloadBody(x, z);
        packet.overrideId(kUnloadChunk19);
        return true;
    }
    io::ByteBuffer column = io::ByteBuffer::wrap(in.readBytes(static_cast<std::size_t>(length)));
    io::ByteBuffer data;
    if (in.failed() || !convertColumn(column, data, mask, full, skyLight_)) {
        return false;
    }
    packet.body = chunkBody(x, z, full, mask, data);
    return true;
}

bool Link47Up::chunkBulk(MutablePacket& packet) const {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    const bool sky = pv::readBoolean(in);
    const std::int32_t count = pv::readCollectionSize(in, 10);

    struct Column {
        std::int32_t x;
        std::int32_t z;
        std::int32_t mask;
    };

    std::vector<Column> columns;
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        const std::int32_t x = in.readInt();
        const std::int32_t z = in.readInt();
        columns.push_back(Column{x, z, static_cast<std::uint16_t>(in.readShort())});
    }
    if (in.failed() || columns.empty()) {
        return false;
    }
    for (std::size_t index = 0; index < columns.size(); ++index) {
        io::ByteBuffer data;
        if (!convertColumn(in, data, columns[index].mask, true, sky)) {
            return false;
        }
        std::vector<std::uint8_t> body = chunkBody(columns[index].x, columns[index].z, true, columns[index].mask, data);
        if (index == 0) {
            packet.body = std::move(body);
            packet.overrideId(kChunk19);
        } else {
            packet.forwarded.emplace_back(kChunk19, std::move(body));
        }
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
