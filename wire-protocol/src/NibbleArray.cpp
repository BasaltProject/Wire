#include <basalt/wire/protocol/play/NibbleArray.hpp>

#include <cassert>

namespace basalt::wire::protocol::play::nibbleArray {

void pack(std::span<const std::int32_t> values, std::span<std::uint8_t> into) {
    assert(values.size() == 4096 && into.size() == kSectionByteCount);
    for (std::size_t i = 0; i < kSectionByteCount; ++i) {
        const auto low = static_cast<std::uint8_t>(values[i * 2] & 0xF);
        const auto high = static_cast<std::uint8_t>(values[i * 2 + 1] & 0xF);
        into[i] = static_cast<std::uint8_t>((high << 4) | low);
    }
}

void unpack(std::span<const std::uint8_t> bytes, std::span<std::int32_t> into) {
    assert(bytes.size() == kSectionByteCount && into.size() == 4096);
    for (std::size_t i = 0; i < kSectionByteCount; ++i) {
        into[i * 2] = bytes[i] & 0xF;
        into[i * 2 + 1] = (bytes[i] >> 4) & 0xF;
    }
}

std::array<std::uint8_t, kSectionByteCount> uniform(std::int32_t level) {
    const auto nibble = static_cast<std::uint8_t>(level & 0xF);
    std::array<std::uint8_t, kSectionByteCount> out{};
    out.fill(static_cast<std::uint8_t>((nibble << 4) | nibble));
    return out;
}

} // namespace basalt::wire::protocol::play::nibbleArray
