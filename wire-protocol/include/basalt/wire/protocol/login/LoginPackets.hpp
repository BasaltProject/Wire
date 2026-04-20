#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::protocol::login {

/// parity: ServerboundHelloPacket. Used for both.
struct LoginStartPacket {
    static constexpr std::int32_t kId = 0;
    static constexpr std::int32_t kMaxNameLength = 16;

    std::string name;
    common::Uuid profileId;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<LoginStartPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const LoginStartPacket&, const LoginStartPacket&) = default;
};

/// parity: ClientboundHelloPacket.
struct HelloPacket {
    static constexpr std::int32_t kId = 1;
    static constexpr std::int32_t kMaxServerIdLength = 20;

    std::string serverId;
    std::vector<std::uint8_t> publicKey;
    std::vector<std::uint8_t> challenge;
    bool shouldAuthenticate = true;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<HelloPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const HelloPacket&, const HelloPacket&) = default;
};

/// parity: ServerboundKeyPacket.
struct KeyPacket {
    static constexpr std::int32_t kId = 1;

    std::vector<std::uint8_t> encryptedSecret;
    std::vector<std::uint8_t> encryptedChallenge;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<KeyPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const KeyPacket&, const KeyPacket&) = default;
};

struct LoginCompressionPacket {
    static constexpr std::int32_t kId = 3;

    std::int32_t threshold = -1;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<LoginCompressionPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const LoginCompressionPacket&, const LoginCompressionPacket&) = default;
};

struct GameProfileProperty {
    std::string name;
    std::string value;
    std::optional<std::string> signature;

    friend bool operator==(const GameProfileProperty&, const GameProfileProperty&) = default;
};

struct GameProfile {
    common::Uuid id;
    std::string name;
    std::vector<GameProfileProperty> properties;

    friend bool operator==(const GameProfile&, const GameProfile&) = default;
};

/// parity: ClientboundLoginFinishedPacket.
struct LoginFinishedPacket {
    static constexpr std::int32_t kId = 2;

    GameProfile gameProfile;
    common::Uuid sessionId;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<LoginFinishedPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const LoginFinishedPacket&, const LoginFinishedPacket&) = default;
};

inline constexpr std::int32_t kLoginAcknowledgedId = 3;

struct LoginDisconnectPacket {
    static constexpr std::int32_t kId = 0;

    text::Component reason;

    void write(io::ByteBuffer& buffer) const;
};

} // namespace basalt::wire::protocol::login
