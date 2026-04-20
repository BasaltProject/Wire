#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::io {

enum class ZipError : std::uint8_t {
    NotAZipFile,
    CorruptDirectory,
    UnsupportedCompression,
    EntryNotFound,
    DecompressionFailed,
};

class ZipReader {
public:
    [[nodiscard]] static std::expected<ZipReader, ZipError> open(std::vector<std::uint8_t> data);

    [[nodiscard]] const std::vector<std::string>& entryNames() const noexcept { return names_; }

    [[nodiscard]] bool contains(std::string_view name) const noexcept;

    [[nodiscard]] std::expected<std::vector<std::uint8_t>, ZipError> read(std::string_view name) const;

private:
    struct Entry {
        std::string name;
        std::uint16_t compressionMethod = 0;
        std::uint32_t compressedSize = 0;
        std::uint32_t uncompressedSize = 0;
        std::uint32_t localHeaderOffset = 0;
    };

    ZipReader() = default;

    [[nodiscard]] const Entry* findEntry(std::string_view name) const noexcept;

    std::vector<std::uint8_t> data_;
    std::vector<Entry> entries_;
    std::vector<std::string> names_;
};

} // namespace basalt::wire::io
