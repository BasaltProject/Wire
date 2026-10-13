#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace basalt::wire::multiversion::java {

/// A network NBT value of a server below 1.20.2: the root carries a name, always empty.
struct NamedNbt {
    nbt::NbtTag tag;
    std::span<const std::uint8_t> raw;
};

[[nodiscard]] inline std::optional<NamedNbt> readNamedNbt(io::ByteBuffer& in) {
    const std::span<const std::uint8_t> bytes = in.readable();
    if (bytes.empty()) {
        in.markFailed();
        return std::nullopt;
    }
    if (bytes[0] == 0) {
        return NamedNbt{nbt::NbtTag{}, in.readSpan(1)};
    }
    if (bytes.size() < 3) {
        in.markFailed();
        return std::nullopt;
    }
    const std::size_t header = 3 + ((static_cast<std::size_t>(bytes[1]) << 8) | bytes[2]);
    if (bytes.size() < header) {
        in.markFailed();
        return std::nullopt;
    }
    std::vector<std::uint8_t> window;
    window.reserve(bytes.size() - header + 1);
    window.push_back(bytes[0]);
    window.insert(window.end(), bytes.begin() + static_cast<std::ptrdiff_t>(header), bytes.end());
    auto parsed = nbt::nbtIO::readAnonymous(window);
    if (!parsed) {
        in.markFailed();
        return std::nullopt;
    }
    return NamedNbt{std::move(parsed->tag), in.readSpan(header + parsed->bytesConsumed - 1)};
}

inline void writeNamedNbt(io::ByteBuffer& out, const nbt::NbtTag& tag) {
    const std::vector<std::uint8_t> bytes = nbt::nbtIO::writeAnonymous(tag);
    out.writeByte(bytes[0]);
    if (bytes[0] != 0) {
        out.writeShort(0);
    }
    out.writeBytes(std::span<const std::uint8_t>{bytes}.subspan(1));
}

} // namespace basalt::wire::multiversion::java
