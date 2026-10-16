#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::io {

/// Writes a ZIP archive whose bytes depend only on its entries: every entry carries the 1980 DOS epoch.
class ZipWriter {
public:
    /// Entries that deflate smaller are deflated; the rest are stored.
    void add(std::string_view name, std::span<const std::uint8_t> data);

    [[nodiscard]] std::vector<std::uint8_t> finish() const;

private:
    struct Entry {
        std::string name;
        std::uint16_t method = 0;
        std::uint32_t crc = 0;
        std::uint32_t uncompressedSize = 0;
        std::vector<std::uint8_t> body;
    };

    std::vector<Entry> entries_;
};

} // namespace basalt::wire::io
