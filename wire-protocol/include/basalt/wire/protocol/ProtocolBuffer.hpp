#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// Protocol-level read and write helpers over the network buffer. Strings are plain UTF-8 with a
/// varint byte-length prefix, unlike NBT's modified UTF-8 (`net.minecraft.network.Utf8String`).
/// Reads never throw: a malformed field raises the buffer's failure flag and returns a default, and
/// the caller checks once per packet.
namespace basalt::wire::protocol {

inline constexpr std::int32_t kDefaultMaxStringLength = 32767;

void writeString(io::ByteBuffer& buffer, std::string_view value);

[[nodiscard]] std::string readString(io::ByteBuffer& buffer, std::int32_t maxLength = kDefaultMaxStringLength);

inline void writeBoolean(io::ByteBuffer& buffer, bool value) { buffer.writeByte(value ? 1 : 0); }

[[nodiscard]] inline bool readBoolean(io::ByteBuffer& buffer) noexcept { return buffer.readByte() != 0; }

inline void writeByteArray(io::ByteBuffer& buffer, std::span<const std::uint8_t> bytes) {
    varInt::write(buffer, static_cast<std::int32_t>(bytes.size()));
    buffer.writeBytes(bytes);
}

/// Bounded against the buffer's own readable bytes by default, mirroring FriendlyByteBuf.
[[nodiscard]] std::vector<std::uint8_t> readByteArray(io::ByteBuffer& buffer, std::int32_t maxSize = -1);

inline void writeUuid(io::ByteBuffer& buffer, const common::Uuid& uuid) {
    buffer.writeLong(static_cast<std::int64_t>(uuid.high()));
    buffer.writeLong(static_cast<std::int64_t>(uuid.low()));
}

[[nodiscard]] inline common::Uuid readUuid(io::ByteBuffer& buffer) noexcept {
    const auto high = static_cast<std::uint64_t>(buffer.readLong());
    const auto low = static_cast<std::uint64_t>(buffer.readLong());
    return common::Uuid{high, low};
}

/// parity: ByteBufCodecs.BIT_SET, `BitSet.toByteArray` behind a varint length. The words are
/// `toLongArray`'s, and a read gives them back with the trailing zero words dropped.
void writeBitSet(io::ByteBuffer& buffer, std::span<const std::int64_t> words);

[[nodiscard]] std::vector<std::int64_t> readBitSet(io::ByteBuffer& buffer);

/// "Network NBT": a root tag with no name field, which is what chat, registry data and disconnect
/// reasons carry.
void writeAnonymousNbt(io::ByteBuffer& buffer, const nbt::NbtTag& tag);

[[nodiscard]] nbt::NbtTag readAnonymousNbt(io::ByteBuffer& buffer);

/// A varint-prefixed count, refused when it claims more entries than the buffer could possibly
/// hold. Every list in the protocol reads its length through this.
[[nodiscard]] std::int32_t readCollectionSize(io::ByteBuffer& buffer, std::int32_t bytesPerEntry = 1) noexcept;

} // namespace basalt::wire::protocol
