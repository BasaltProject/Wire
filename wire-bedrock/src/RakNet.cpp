#include <basalt/wire/bedrock/RakNet.hpp>

#include <algorithm>

namespace basalt::wire::bedrock {

void writeMediumLE(io::ByteBuffer& buffer, std::uint32_t value) {
    buffer.writeByte(static_cast<std::uint8_t>(value & 0xffU));
    buffer.writeByte(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    buffer.writeByte(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
}

std::uint32_t readMediumLE(io::ByteBuffer& buffer) noexcept {
    const std::uint32_t low = buffer.readByte();
    const std::uint32_t middle = buffer.readByte();
    const std::uint32_t high = buffer.readByte();
    return low | (middle << 8U) | (high << 16U);
}

void writeAddress(io::ByteBuffer& buffer, const io::UdpEndpoint& endpoint) {
    buffer.writeByte(4);
    for (int octet = 0; octet < 4; ++octet) {
        const auto shifted = static_cast<std::uint8_t>((endpoint.address >> (octet * 8)) & 0xffU);
        buffer.writeByte(static_cast<std::uint8_t>(~shifted));
    }
    buffer.writeShort(static_cast<std::int16_t>(endpoint.port));
}

io::UdpEndpoint readAddress(io::ByteBuffer& buffer) noexcept {
    io::UdpEndpoint endpoint{};
    const std::uint8_t version = buffer.readByte();
    if (version == 4) {
        std::uint32_t address = 0;
        for (int octet = 0; octet < 4; ++octet) {
            const auto value = static_cast<std::uint32_t>(static_cast<std::uint8_t>(~buffer.readByte()));
            address |= value << (octet * 8);
        }
        endpoint.address = address;
        endpoint.port = static_cast<std::uint16_t>(buffer.readShort());
        return endpoint;
    }
    // A client announces its own addresses too, and a machine with IPv6 sends them.
    buffer.skip(28);
    return endpoint;
}

bool hasMagic(std::span<const std::uint8_t> bytes) noexcept {
    return bytes.size() >= kMagic.size() && std::equal(kMagic.begin(), kMagic.end(), bytes.begin());
}

} // namespace basalt::wire::bedrock
