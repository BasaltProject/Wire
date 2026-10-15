#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/io/ZipReader.hpp>

#include <cstdint>

namespace basalt::wire::io {
namespace {

constexpr std::uint32_t kLocalFileHeaderSignature = 0x04034b50U;
constexpr std::uint32_t kCentralDirectorySignature = 0x02014b50U;
constexpr std::uint32_t kEndOfCentralDirectorySignature = 0x06054b50U;
constexpr std::size_t kEocdFixedSize = 22;
constexpr std::size_t kMaxCommentLength = 65535;
constexpr std::uint16_t kCompressionStored = 0;
constexpr std::uint16_t kCompressionDeflate = 8;

std::optional<std::uint16_t> readU16(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset + 2 > data.size()) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

std::optional<std::uint32_t> readU32(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset + 4 > data.size()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(data[offset]) | (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) | (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

std::optional<std::size_t> findEndOfCentralDirectory(std::span<const std::uint8_t> data) {
    if (data.size() < kEocdFixedSize) {
        return std::nullopt;
    }
    const std::size_t last = data.size() - kEocdFixedSize;
    const std::size_t first = last > kMaxCommentLength ? last - kMaxCommentLength : 0;
    for (std::size_t offset = last + 1; offset-- > first;) {
        if (readU32(data, offset) == kEndOfCentralDirectorySignature) {
            return offset;
        }
    }
    return std::nullopt;
}

} // namespace

std::expected<ZipReader, ZipError> ZipReader::open(std::vector<std::uint8_t> data) {
    ZipReader reader;
    reader.data_ = std::move(data);
    const std::span<const std::uint8_t> bytes{reader.data_};

    const std::optional<std::size_t> eocd = findEndOfCentralDirectory(bytes);
    if (!eocd) {
        return std::unexpected{ZipError::NotAZipFile};
    }

    const std::optional<std::uint16_t> entryCount = readU16(bytes, *eocd + 10);
    const std::optional<std::uint32_t> directoryOffset = readU32(bytes, *eocd + 16);
    if (!entryCount || !directoryOffset) {
        return std::unexpected{ZipError::CorruptDirectory};
    }

    reader.entries_.reserve(*entryCount);
    reader.names_.reserve(*entryCount);
    std::size_t offset = *directoryOffset;
    for (std::uint16_t i = 0; i < *entryCount; ++i) {
        if (readU32(bytes, offset) != kCentralDirectorySignature) {
            return std::unexpected{ZipError::CorruptDirectory};
        }
        const auto method = readU16(bytes, offset + 10);
        const auto compressedSize = readU32(bytes, offset + 20);
        const auto uncompressedSize = readU32(bytes, offset + 24);
        const auto nameLength = readU16(bytes, offset + 28);
        const auto extraLength = readU16(bytes, offset + 30);
        const auto commentLength = readU16(bytes, offset + 32);
        const auto localHeaderOffset = readU32(bytes, offset + 42);
        if (!method || !compressedSize || !uncompressedSize || !nameLength || !extraLength || !commentLength ||
            !localHeaderOffset || offset + 46 + *nameLength > bytes.size()) {
            return std::unexpected{ZipError::CorruptDirectory};
        }

        Entry entry;
        entry.name.assign(reinterpret_cast<const char*>(bytes.data() + offset + 46), *nameLength);
        entry.compressionMethod = *method;
        entry.compressedSize = *compressedSize;
        entry.uncompressedSize = *uncompressedSize;
        entry.localHeaderOffset = *localHeaderOffset;
        reader.names_.push_back(entry.name);
        reader.entries_.push_back(std::move(entry));

        offset += 46 + *nameLength + *extraLength + *commentLength;
    }
    return reader;
}

const ZipReader::Entry* ZipReader::findEntry(std::string_view name) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

bool ZipReader::contains(std::string_view name) const noexcept { return findEntry(name) != nullptr; }

std::expected<std::vector<std::uint8_t>, ZipError> ZipReader::read(std::string_view name) const {
    const Entry* entry = findEntry(name);
    if (entry == nullptr) {
        return std::unexpected{ZipError::EntryNotFound};
    }

    const std::span<const std::uint8_t> bytes{data_};
    const std::size_t header = entry->localHeaderOffset;
    if (readU32(bytes, header) != kLocalFileHeaderSignature) {
        return std::unexpected{ZipError::CorruptDirectory};
    }
    const auto nameLength = readU16(bytes, header + 26);
    const auto extraLength = readU16(bytes, header + 28);
    if (!nameLength || !extraLength) {
        return std::unexpected{ZipError::CorruptDirectory};
    }

    const std::size_t start = header + 30 + *nameLength + *extraLength;
    if (start + entry->compressedSize > bytes.size()) {
        return std::unexpected{ZipError::CorruptDirectory};
    }
    const std::span<const std::uint8_t> body = bytes.subspan(start, entry->compressedSize);

    if (entry->compressionMethod == kCompressionStored) {
        return std::vector<std::uint8_t>{body.begin(), body.end()};
    }
    if (entry->compressionMethod != kCompressionDeflate) {
        return std::unexpected{ZipError::UnsupportedCompression};
    }

    auto inflated = rawDeflate::decompress(body, entry->uncompressedSize);
    if (!inflated) {
        return std::unexpected{ZipError::DecompressionFailed};
    }
    return std::move(*inflated);
}

} // namespace basalt::wire::io
