#include <basalt/wire/multiversion/CompactBitStorage.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::compactBitStorage {

std::vector<std::int32_t> unpack(std::span<const std::int64_t> words, int bits, std::size_t size) {
    std::vector<std::int32_t> values(size);
    const std::uint64_t mask = (1ULL << bits) - 1;

    for (std::size_t index = 0; index < size; ++index) {
        const std::size_t bit = index * static_cast<std::size_t>(bits);
        const std::size_t slot = bit / 64;
        const std::size_t offset = bit % 64;

        std::uint64_t value = (static_cast<std::uint64_t>(words[slot]) >> offset) & mask;
        // The entry spilled into the next word, which is exactly what 1.16 stopped doing.
        const std::ptrdiff_t spilled = static_cast<std::ptrdiff_t>(offset) + bits - 64;
        if (spilled > 0) {
            const std::uint64_t tail = static_cast<std::uint64_t>(words[slot + 1]) & ((1ULL << spilled) - 1);
            value |= tail << (bits - spilled);
        }
        values[index] = static_cast<std::int32_t>(value);
    }
    return values;
}

std::vector<std::int64_t> pack(std::span<const std::int32_t> values, int bits) {
    std::vector<std::int64_t> words(longsFor(values.size(), bits), 0);
    const std::uint64_t mask = (1ULL << bits) - 1;

    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::size_t bit = index * static_cast<std::size_t>(bits);
        const std::size_t slot = bit / 64;
        const std::size_t offset = bit % 64;

        const std::uint64_t value = static_cast<std::uint64_t>(static_cast<std::uint32_t>(values[index])) & mask;
        words[slot] = static_cast<std::int64_t>(static_cast<std::uint64_t>(words[slot]) | (value << offset));

        const std::ptrdiff_t spilled = static_cast<std::ptrdiff_t>(offset) + bits - 64;
        if (spilled > 0) {
            words[slot + 1] =
                static_cast<std::int64_t>(static_cast<std::uint64_t>(words[slot + 1]) | (value >> (bits - spilled)));
        }
    }
    return words;
}

} // namespace basalt::wire::multiversion::compactBitStorage
