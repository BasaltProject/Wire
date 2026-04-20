#pragma once

#include <basalt/wire/nbt/NbtType.hpp>

#include <bit>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace basalt::wire::nbt {

/// Every read is bounds checked and returns a value, never throws: this parses hostile input.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] std::size_t position() const noexcept { return position_; }

    [[nodiscard]] bool hasRemaining() const noexcept { return position_ < bytes_.size(); }

    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - position_; }

    [[nodiscard]] std::expected<std::uint8_t, NbtError> readByte() noexcept {
        if (position_ >= bytes_.size()) {
            return std::unexpected{NbtError::UnexpectedEnd};
        }
        return bytes_[position_++];
    }

    [[nodiscard]] std::expected<std::int16_t, NbtError> readShort() noexcept {
        return readUnsigned<std::uint16_t>().transform([](std::uint16_t v) { return static_cast<std::int16_t>(v); });
    }

    [[nodiscard]] std::expected<std::int32_t, NbtError> readInt() noexcept {
        return readUnsigned<std::uint32_t>().transform([](std::uint32_t v) { return static_cast<std::int32_t>(v); });
    }

    [[nodiscard]] std::expected<std::int64_t, NbtError> readLong() noexcept {
        return readUnsigned<std::uint64_t>().transform([](std::uint64_t v) { return static_cast<std::int64_t>(v); });
    }

    [[nodiscard]] std::expected<float, NbtError> readFloat() noexcept {
        return readUnsigned<std::uint32_t>().transform([](std::uint32_t v) { return std::bit_cast<float>(v); });
    }

    [[nodiscard]] std::expected<double, NbtError> readDouble() noexcept {
        return readUnsigned<std::uint64_t>().transform([](std::uint64_t v) { return std::bit_cast<double>(v); });
    }

private:
    template <typename U>
    [[nodiscard]] std::expected<U, NbtError> readUnsigned() noexcept {
        constexpr std::size_t width = sizeof(U);
        if (remaining() < width) {
            return std::unexpected{NbtError::UnexpectedEnd};
        }
        U result = 0;
        for (std::size_t i = 0; i < width; ++i) {
            result = static_cast<U>((result << 8) | bytes_[position_ + i]);
        }
        position_ += width;
        return result;
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
};

class ByteWriter {
public:
    explicit ByteWriter(std::size_t initialCapacity = 64) { buffer_.reserve(initialCapacity); }

    void writeByte(std::uint8_t value) { buffer_.push_back(value); }

    void writeShort(std::int16_t value) { writeUnsigned(static_cast<std::uint16_t>(value)); }

    void writeInt(std::int32_t value) { writeUnsigned(static_cast<std::uint32_t>(value)); }

    void writeLong(std::int64_t value) { writeUnsigned(static_cast<std::uint64_t>(value)); }

    void writeFloat(float value) { writeUnsigned(std::bit_cast<std::uint32_t>(value)); }

    void writeDouble(double value) { writeUnsigned(std::bit_cast<std::uint64_t>(value)); }

    [[nodiscard]] std::vector<std::uint8_t> take() noexcept { return std::move(buffer_); }

    [[nodiscard]] std::size_t size() const noexcept { return buffer_.size(); }

private:
    template <typename U>
    void writeUnsigned(U value) {
        for (std::size_t i = sizeof(U); i > 0; --i) {
            buffer_.push_back(static_cast<std::uint8_t>(value >> ((i - 1) * 8)));
        }
    }

    std::vector<std::uint8_t> buffer_;
};

} // namespace basalt::wire::nbt
