#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::protocol::cookie {
inline constexpr std::size_t kMaxPayloadBytes = 5120;

struct RequestPacket {
    static constexpr std::int32_t kLoginId = 5;
    std::string key;
    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RequestPacket> read(io::ByteBuffer& buffer);
    friend bool operator==(const RequestPacket&, const RequestPacket&) = default;
};

struct ResponsePacket {
    static constexpr std::int32_t kLoginId = 4;
    std::string key;
    std::optional<std::vector<std::uint8_t>> payload;
    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ResponsePacket> read(io::ByteBuffer& buffer);
    friend bool operator==(const ResponsePacket&, const ResponsePacket&) = default;
};

struct StorePacket {
    std::string key;
    std::vector<std::uint8_t> payload;
    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<StorePacket> read(io::ByteBuffer& buffer);
    friend bool operator==(const StorePacket&, const StorePacket&) = default;
};
} // namespace basalt::wire::protocol::cookie
