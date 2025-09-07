#pragma once

#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::protocol::play {

/// How many longs a storage of this shape takes.
[[nodiscard]] constexpr std::size_t bitStorageLongCount(int bitsPerEntry, std::size_t size) noexcept {
    if (bitsPerEntry == 0) {
        return 0;
    }
    const std::size_t valuesPerLong = 64 / static_cast<std::size_t>(bitsPerEntry);
    return (size + valuesPerLong - 1) / valuesPerLong;
}

/// Fixed-width values packed into 64-bit words, matching vanilla's SimpleBitStorage: an entry never
/// spans two words, and the leftover high bits of each word are padding. Not the pre-1.16 scheme
/// that packed across word boundaries.
///
/// A view, not an owner. The words belong to the chunk section, and both the world and the network
/// codec read them in place: expanding a section into 4096 ints to send it was 16 KB of garbage per
/// section and 24 sections per chunk.
class BitStorageView {
public:
    BitStorageView(int bitsPerEntry, std::size_t size, std::span<std::uint64_t> words) noexcept
        : words_(words), size_(size), bitsPerEntry_(bitsPerEntry),
          valuesPerLong_(bitsPerEntry == 0 ? std::size_t{0} : 64 / static_cast<std::size_t>(bitsPerEntry)),
          mask_(bitsPerEntry == 0 ? 0 : (1ULL << bitsPerEntry) - 1) {
        assert(bitsPerEntry >= 0 && bitsPerEntry <= 32);
        assert(words.size() == bitStorageLongCount(bitsPerEntry, size));
    }

    [[nodiscard]] int bitsPerEntry() const noexcept { return bitsPerEntry_; }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    [[nodiscard]] std::span<const std::uint64_t> words() const noexcept { return words_; }

    [[nodiscard]] std::int32_t get(std::size_t index) const noexcept {
        if (bitsPerEntry_ == 0) {
            return 0;
        }
        const std::size_t word = index / valuesPerLong_;
        const std::size_t offset = (index % valuesPerLong_) * static_cast<std::size_t>(bitsPerEntry_);
        return static_cast<std::int32_t>((words_[word] >> offset) & mask_);
    }

    void set(std::size_t index, std::int32_t value) noexcept {
        if (bitsPerEntry_ == 0) {
            return;
        }
        const std::size_t word = index / valuesPerLong_;
        const std::size_t offset = (index % valuesPerLong_) * static_cast<std::size_t>(bitsPerEntry_);
        const std::uint64_t bits = static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)) & mask_;
        words_[word] = (words_[word] & ~(mask_ << offset)) | (bits << offset);
    }

private:
    std::span<std::uint64_t> words_;
    std::size_t size_;
    int bitsPerEntry_;
    std::size_t valuesPerLong_;
    std::uint64_t mask_;
};

/// A BitStorageView that owns its words, for the callers that do not already have storage.
class BitStorage {
public:
    BitStorage(int bitsPerEntry, std::size_t size)
        : words_(bitStorageLongCount(bitsPerEntry, size), 0), size_(size), bitsPerEntry_(bitsPerEntry) {}

    BitStorage(int bitsPerEntry, std::size_t size, std::vector<std::uint64_t> words)
        : words_(std::move(words)), size_(size), bitsPerEntry_(bitsPerEntry) {}

    [[nodiscard]] BitStorageView view() noexcept { return BitStorageView{bitsPerEntry_, size_, words_}; }

    [[nodiscard]] std::int32_t get(std::size_t index) const noexcept {
        return BitStorageView{bitsPerEntry_, size_, const_cast<std::vector<std::uint64_t>&>(words_)}.get(index);
    }

    void set(std::size_t index, std::int32_t value) noexcept { view().set(index, value); }

    [[nodiscard]] std::span<const std::uint64_t> words() const noexcept { return words_; }

    [[nodiscard]] int bitsPerEntry() const noexcept { return bitsPerEntry_; }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
    std::vector<std::uint64_t> words_;
    std::size_t size_;
    int bitsPerEntry_;
};

} // namespace basalt::wire::protocol::play
