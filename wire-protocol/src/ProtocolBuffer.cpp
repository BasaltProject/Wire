#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::wire::protocol {

void writeString(io::ByteBuffer& buffer, std::string_view value) {
    varInt::write(buffer, static_cast<std::int32_t>(value.size()));
    buffer.writeBytes(std::span{reinterpret_cast<const std::uint8_t*>(value.data()), value.size()});
}

std::string readString(io::ByteBuffer& buffer, std::int32_t maxLength) {
    const std::int32_t length = varInt::read(buffer);
    // Vanilla bounds the encoded length at three bytes per character before it reads anything, so a
    // crafted length cannot make the server reserve for a string it never sent.
    if (length < 0 || length > maxLength * 3) {
        buffer.markFailed();
        return {};
    }
    const std::span<const std::uint8_t> bytes = buffer.readSpan(static_cast<std::size_t>(length));
    if (buffer.failed()) {
        return {};
    }
    return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

std::vector<std::uint8_t> readByteArray(io::ByteBuffer& buffer, std::int32_t maxSize) {
    const std::int32_t bound = maxSize < 0 ? static_cast<std::int32_t>(buffer.readableBytes()) : maxSize;
    const std::int32_t length = varInt::read(buffer);
    if (length < 0 || length > bound) {
        buffer.markFailed();
        return {};
    }
    return buffer.readBytes(static_cast<std::size_t>(length));
}

void writeAnonymousNbt(io::ByteBuffer& buffer, const nbt::NbtTag& tag) {
    buffer.writeBytes(nbt::nbtIO::writeAnonymous(tag));
}

nbt::NbtTag readAnonymousNbt(io::ByteBuffer& buffer) {
    const auto parsed = nbt::nbtIO::readAnonymous(buffer.readable());
    if (!parsed) {
        buffer.markFailed();
        return {};
    }
    buffer.skip(parsed->bytesConsumed);
    return parsed->tag;
}

std::int32_t readCollectionSize(io::ByteBuffer& buffer, std::int32_t bytesPerEntry) noexcept {
    const std::int32_t size = varInt::read(buffer);
    if (size < 0) {
        buffer.markFailed();
        return 0;
    }
    // Even a one-byte entry has to be in the buffer already, so a count larger than what is left is
    // a lie and reserving for it is how a small packet turns into a large allocation.
    const std::int32_t maximum = static_cast<std::int32_t>(buffer.readableBytes()) / std::max(bytesPerEntry, 1);
    if (size > maximum) {
        buffer.markFailed();
        return 0;
    }
    return size;
}

} // namespace basalt::wire::protocol
