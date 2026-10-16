#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/io/ZipWriter.hpp>

#include <array>

namespace basalt::wire::io {
namespace {

constexpr std::uint16_t kStored = 0;
constexpr std::uint16_t kDeflated = 8;
constexpr std::uint16_t kUtf8Name = 0x0800;
constexpr std::uint16_t kDosDate1980 = 0x0021;

[[nodiscard]] std::uint32_t crc32(std::span<const std::uint8_t> data) {
    static const std::array<std::uint32_t, 256> kTable = [] {
        std::array<std::uint32_t, 256> table{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t value = i;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1U) != 0 ? (value >> 1U) ^ 0xEDB88320U : value >> 1U;
            }
            table[i] = value;
        }
        return table;
    }();
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::uint8_t byte : data) {
        crc = kTable[(crc ^ byte) & 0xFFU] ^ (crc >> 8U);
    }
    return crc ^ 0xFFFFFFFFU;
}

void put16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    put16(out, static_cast<std::uint16_t>(value & 0xFFFFU));
    put16(out, static_cast<std::uint16_t>(value >> 16U));
}

} // namespace

void ZipWriter::add(std::string_view name, std::span<const std::uint8_t> data) {
    Entry entry{.name = std::string{name},
                .method = kStored,
                .crc = crc32(data),
                .uncompressedSize = static_cast<std::uint32_t>(data.size()),
                .body = std::vector<std::uint8_t>(data.begin(), data.end())};
    if (const auto deflated = rawDeflate::compress(data); deflated.has_value() && deflated->size() < data.size()) {
        entry.method = kDeflated;
        entry.body = *deflated;
    }
    entries_.push_back(std::move(entry));
}

std::vector<std::uint8_t> ZipWriter::finish() const {
    std::vector<std::uint8_t> out;
    std::vector<std::uint32_t> offsets;
    for (const Entry& entry : entries_) {
        offsets.push_back(static_cast<std::uint32_t>(out.size()));
        put32(out, 0x04034b50);
        put16(out, 20);
        put16(out, kUtf8Name);
        put16(out, entry.method);
        put16(out, 0);
        put16(out, kDosDate1980);
        put32(out, entry.crc);
        put32(out, static_cast<std::uint32_t>(entry.body.size()));
        put32(out, entry.uncompressedSize);
        put16(out, static_cast<std::uint16_t>(entry.name.size()));
        put16(out, 0);
        out.insert(out.end(), entry.name.begin(), entry.name.end());
        out.insert(out.end(), entry.body.begin(), entry.body.end());
    }
    const auto directoryOffset = static_cast<std::uint32_t>(out.size());
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const Entry& entry = entries_[i];
        put32(out, 0x02014b50);
        put16(out, 20);
        put16(out, 20);
        put16(out, kUtf8Name);
        put16(out, entry.method);
        put16(out, 0);
        put16(out, kDosDate1980);
        put32(out, entry.crc);
        put32(out, static_cast<std::uint32_t>(entry.body.size()));
        put32(out, entry.uncompressedSize);
        put16(out, static_cast<std::uint16_t>(entry.name.size()));
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, 0);
        put32(out, offsets[i]);
        out.insert(out.end(), entry.name.begin(), entry.name.end());
    }
    const auto directorySize = static_cast<std::uint32_t>(out.size() - directoryOffset);
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, static_cast<std::uint16_t>(entries_.size()));
    put16(out, static_cast<std::uint16_t>(entries_.size()));
    put32(out, directorySize);
    put32(out, directoryOffset);
    put16(out, 0);
    return out;
}

} // namespace basalt::wire::io
