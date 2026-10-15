#pragma once

#include <basalt/wire/io/detail/ByteStorage.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace basalt::wire::io {

/// A growable, big-endian byte buffer with independent read and write cursors, in the spirit of
/// Netty's ByteBuf. This is what the packet codecs are built on.
///
/// Reading past the end neither throws nor reads out of bounds: it returns zero and raises a sticky
/// failure flag the caller checks once per packet. Packet decoding is the main attack surface, and a
/// codec layer this size cannot afford a branch on every read in the source, only in machine code.
class ByteBuffer {
public:
    explicit ByteBuffer(std::size_t initialCapacity = 256) { storage_.reserve(initialCapacity); }

    /// Resource must outlive this buffer and any managed copies. Allocation remains owner-only.
    ByteBuffer(std::size_t initialCapacity, std::pmr::memory_resource& resource) : storage_(resource) {
        storage_.reserve(initialCapacity);
    }

    /// Adopts the bytes as readable content without copying.
    [[nodiscard]] static ByteBuffer wrap(std::vector<std::uint8_t> bytes) {
        ByteBuffer buffer{0};
        buffer.storage_.adopt(std::move(bytes));
        return buffer;
    }

    [[nodiscard]] std::size_t readerIndex() const noexcept { return readerIndex_; }

    [[nodiscard]] std::size_t writerIndex() const noexcept { return storage_.size(); }

    [[nodiscard]] std::size_t readableBytes() const noexcept { return storage_.size() - readerIndex_; }

    [[nodiscard]] std::size_t capacity() const noexcept { return storage_.capacity(); }

    /// True once a read has asked for more than the buffer held. Sticky until cleared.
    [[nodiscard]] bool failed() const noexcept { return failed_; }

    void clearFailure() noexcept { failed_ = false; }

    /// Raised by the layers above when the bytes were there but did not mean anything: a varint
    /// longer than five bytes, a string past its declared maximum.
    void markFailed() noexcept { failed_ = true; }

    void reserve(std::size_t extraBytes) { storage_.reserve(storage_.size() + extraBytes); }

    [[nodiscard]] bool tryReserve(std::size_t extraBytes) noexcept {
        if (extraBytes > std::numeric_limits<std::size_t>::max() - storage_.size())
            return false;
        try {
            reserve(extraBytes);
            return true;
        } catch (const std::bad_alloc&) {
            return false;
        } catch (const std::length_error&) {
            return false;
        }
    }

    void writeByte(std::uint8_t value) { storage_.push_back(value); }

    void writeBytes(std::span<const std::uint8_t> bytes) { storage_.append(bytes); }

    void writeShort(std::int16_t value) { writeUnsigned(static_cast<std::uint16_t>(value)); }

    void writeInt(std::int32_t value) { writeUnsigned(static_cast<std::uint32_t>(value)); }

    void writeLong(std::int64_t value) { writeUnsigned(static_cast<std::uint64_t>(value)); }

    void writeFloat(float value) { writeUnsigned(std::bit_cast<std::uint32_t>(value)); }

    void writeDouble(double value) { writeUnsigned(std::bit_cast<std::uint64_t>(value)); }

    [[nodiscard]] std::uint8_t readByte() noexcept {
        if (readerIndex_ >= storage_.size()) {
            failed_ = true;
            return 0;
        }
        return storage_[readerIndex_++];
    }

    [[nodiscard]] std::int16_t readShort() noexcept { return static_cast<std::int16_t>(readUnsigned<std::uint16_t>()); }

    [[nodiscard]] std::int32_t readInt() noexcept { return static_cast<std::int32_t>(readUnsigned<std::uint32_t>()); }

    [[nodiscard]] std::int64_t readLong() noexcept { return static_cast<std::int64_t>(readUnsigned<std::uint64_t>()); }

    [[nodiscard]] float readFloat() noexcept { return std::bit_cast<float>(readUnsigned<std::uint32_t>()); }

    [[nodiscard]] double readDouble() noexcept { return std::bit_cast<double>(readUnsigned<std::uint64_t>()); }

    [[nodiscard]] std::vector<std::uint8_t> readBytes(std::size_t length) {
        const std::span<const std::uint8_t> view = readSpan(length);
        return std::vector<std::uint8_t>{view.begin(), view.end()};
    }

    /// A view of the next bytes, consumed but never copied. Invalidated by the next write.
    [[nodiscard]] std::span<const std::uint8_t> readSpan(std::size_t length) noexcept {
        if (length > readableBytes()) {
            failed_ = true;
            readerIndex_ = storage_.size();
            return {};
        }
        const std::span<const std::uint8_t> view{storage_.data() + readerIndex_, length};
        readerIndex_ += length;
        return view;
    }

    [[nodiscard]] std::uint8_t peekByte(std::size_t offset = 0) const noexcept {
        return readerIndex_ + offset < storage_.size() ? storage_[readerIndex_ + offset] : 0;
    }

    void skip(std::size_t length) noexcept { static_cast<void>(readSpan(length)); }

    /// The unread region, without consuming it. Invalidated by the next write.
    [[nodiscard]] std::span<const std::uint8_t> readable() const noexcept {
        return {storage_.data() + readerIndex_, readableBytes()};
    }

    [[nodiscard]] std::span<std::uint8_t> writableTail(std::size_t length) {
        const std::size_t at = storage_.size();
        storage_.resize(at + length);
        return {storage_.data() + at, length};
    }

    /// A writable view of everything from an offset to the end, for a layer that has to go back and
    /// fill in a length it could not know when it reserved room for it.
    [[nodiscard]] std::span<std::uint8_t> mutableSpan(std::size_t from) noexcept {
        return {storage_.data() + from, storage_.size() - from};
    }

    /// Drops however much of the tail was not used, after writing straight into `writableTail`.
    void trimTail(std::size_t unused) noexcept { storage_.resize(storage_.size() - unused); }

    /// Moves the unread bytes to the front so the buffer can be reused without reallocating. This is
    /// what keeps a connection's read buffer alive across packets instead of one per packet.
    void discardReadBytes() {
        if (readerIndex_ == 0) {
            return;
        }
        const std::size_t remaining = readableBytes();
        if (remaining > 0) {
            std::memmove(storage_.data(), storage_.data() + readerIndex_, remaining);
        }
        storage_.resize(remaining);
        readerIndex_ = 0;
    }

    void clear() noexcept {
        storage_.clear();
        readerIndex_ = 0;
        failed_ = false;
    }

    [[nodiscard]] std::vector<std::uint8_t> toVector() const {
        const std::span<const std::uint8_t> view = readable();
        return std::vector<std::uint8_t>{view.begin(), view.end()};
    }

private:
    template <typename U>
    void writeUnsigned(U value) {
        std::array<std::uint8_t, sizeof(U)> bytes{};
        for (std::size_t i = 0; i < sizeof(U); ++i) {
            bytes[i] = static_cast<std::uint8_t>(value >> ((sizeof(U) - i - 1) * 8));
        }
        storage_.append(bytes);
    }

    template <typename U>
    [[nodiscard]] U readUnsigned() noexcept {
        if (readableBytes() < sizeof(U)) {
            failed_ = true;
            readerIndex_ = storage_.size();
            return 0;
        }
        U result = 0;
        const auto* bytes = storage_.data() + readerIndex_;
        for (std::size_t i = 0; i < sizeof(U); ++i) {
            result = static_cast<U>((result << 8) | bytes[i]);
        }
        readerIndex_ += sizeof(U);
        return result;
    }

    detail::ByteStorage storage_;
    std::size_t readerIndex_ = 0;
    bool failed_ = false;
};

} // namespace basalt::wire::io
