#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/PalettedContainer.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::wire::protocol::play::palettedContainer {
namespace {

std::int32_t indexIn(std::span<const std::int32_t> palette, std::int32_t value) noexcept {
    for (std::size_t i = 0; i < palette.size(); ++i) {
        if (palette[i] == value) {
            return static_cast<std::int32_t>(i);
        }
    }
    return -1;
}

int bitsFor(std::size_t distinctCount) noexcept {
    int bits = 0;
    while ((1ULL << bits) < distinctCount) {
        ++bits;
    }
    return bits;
}

} // namespace

void writePacked(io::ByteBuffer& buffer, const BitStorageView& storage, std::span<const std::int32_t> palette,
                 int directBits) {
    if (palette.size() == 1) {
        buffer.writeByte(0);
        varInt::write(buffer, palette[0]);
        return;
    }

    buffer.writeByte(static_cast<std::uint8_t>(storage.bitsPerEntry()));
    if (storage.bitsPerEntry() != directBits) {
        varInt::write(buffer, static_cast<std::int32_t>(palette.size()));
        for (const std::int32_t entry : palette) {
            varInt::write(buffer, entry);
        }
    }
    for (const std::uint64_t word : storage.words()) {
        buffer.writeLong(static_cast<std::int64_t>(word));
    }
}

void write(io::ByteBuffer& buffer, std::span<const std::int32_t> values, int minIndirectBits, int maxIndirectBits,
           int directBits) {
    const std::size_t paletteLimit = (1ULL << maxIndirectBits) + 1;
    std::vector<std::int32_t> palette;
    palette.reserve(paletteLimit);
    for (const std::int32_t value : values) {
        if (indexIn(palette, value) < 0) {
            palette.push_back(value);
            if (palette.size() == paletteLimit) {
                break;
            }
        }
    }

    if (palette.size() == 1) {
        buffer.writeByte(0);
        varInt::write(buffer, palette[0]);
        return;
    }

    const int neededBits = bitsFor(palette.size());
    if (neededBits <= maxIndirectBits) {
        const int bitsPerEntry = std::max(neededBits, minIndirectBits);
        BitStorage storage{bitsPerEntry, values.size()};
        for (std::size_t i = 0; i < values.size(); ++i) {
            storage.set(i, indexIn(palette, values[i]));
        }
        writePacked(buffer, storage.view(), palette, directBits);
        return;
    }

    BitStorage storage{directBits, values.size()};
    for (std::size_t i = 0; i < values.size(); ++i) {
        storage.set(i, values[i]);
    }
    buffer.writeByte(static_cast<std::uint8_t>(directBits));
    for (const std::uint64_t word : storage.words()) {
        buffer.writeLong(static_cast<std::int64_t>(word));
    }
}

std::vector<std::int32_t> read(io::ByteBuffer& buffer, std::size_t size, int maxIndirectBits) {
    const int bitsPerEntry = buffer.readByte();
    if (buffer.failed() || bitsPerEntry > 32) {
        buffer.markFailed();
        return {};
    }

    if (bitsPerEntry == 0) {
        const std::int32_t value = varInt::read(buffer);
        return std::vector<std::int32_t>(size, value);
    }

    std::vector<std::int32_t> palette;
    if (bitsPerEntry <= maxIndirectBits) {
        const std::int32_t count = readCollectionSize(buffer);
        palette.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count; ++i) {
            palette.push_back(varInt::read(buffer));
        }
    }

    const std::size_t wordCount = bitStorageLongCount(bitsPerEntry, size);
    std::vector<std::uint64_t> words(wordCount);
    for (std::size_t i = 0; i < wordCount; ++i) {
        words[i] = static_cast<std::uint64_t>(buffer.readLong());
    }
    if (buffer.failed()) {
        return {};
    }

    BitStorage storage{bitsPerEntry, size, std::move(words)};
    std::vector<std::int32_t> values(size);
    for (std::size_t i = 0; i < size; ++i) {
        const std::int32_t raw = storage.get(i);
        if (palette.empty()) {
            values[i] = raw;
        } else if (raw >= 0 && static_cast<std::size_t>(raw) < palette.size()) {
            values[i] = palette[static_cast<std::size_t>(raw)];
        } else {
            buffer.markFailed();
            return {};
        }
    }
    return values;
}

} // namespace basalt::wire::protocol::play::palettedContainer
