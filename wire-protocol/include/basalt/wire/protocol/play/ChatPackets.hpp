#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::protocol::play {

/// parity: `MessageSignature.BYTES`.
inline constexpr std::size_t kMessageSignatureBytes = 256;

using MessageSignature = std::array<std::uint8_t, kMessageSignatureBytes>;

/// parity: `LastSeenMessages.Update`.
struct LastSeenUpdate {
    /// parity: `LastSeenMessages.LAST_SEEN_MESSAGES_MAX_LENGTH`.
    static constexpr std::size_t kWindow = 20;
    static constexpr std::size_t kBytes = (kWindow + 7) / 8;

    std::int32_t offset = 0;
    std::array<std::uint8_t, kBytes> acknowledged{};
    /// Zero means the client asked for no check.
    std::int8_t checksum = 0;

    [[nodiscard]] bool isAcknowledged(std::size_t index) const noexcept {
        return index / 8 < kBytes && ((acknowledged[index / 8] >> (index % 8)) & 1) != 0;
    }

    void setAcknowledged(std::size_t index) noexcept {
        acknowledged[index / 8] = static_cast<std::uint8_t>(acknowledged[index / 8] | (1U << (index % 8)));
    }

    [[nodiscard]] std::size_t bitLength() const noexcept;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static LastSeenUpdate read(io::ByteBuffer& buffer);

    friend bool operator==(const LastSeenUpdate&, const LastSeenUpdate&) = default;
};

struct RemoteChatSessionData {
    /// parity: `ByteBufCodecs.MAX_PUBLIC_KEY_LENGTH` and `ProfilePublicKey.Data.MAX_KEY_SIGNATURE_SIZE`.
    static constexpr std::int32_t kMaxPublicKeyBytes = 512;
    static constexpr std::int32_t kMaxKeySignatureBytes = 4096;

    common::Uuid sessionId;
    std::int64_t expiresAt = 0;
    std::vector<std::uint8_t> publicKey;
    std::vector<std::uint8_t> keySignature;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static RemoteChatSessionData read(io::ByteBuffer& buffer);

    friend bool operator==(const RemoteChatSessionData&, const RemoteChatSessionData&) = default;
};

/// Serverbound, id 0x06: the client moved its acknowledgement window without saying anything.
struct ChatAckPacket {
    static constexpr std::int32_t kId = 0x06;

    std::int32_t offset = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChatAckPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChatAckPacket&, const ChatAckPacket&) = default;
};

/// Serverbound, id 0x09: a chat message, signed when the client has a chat session.
struct ChatMessagePacket {
    static constexpr std::int32_t kId = 0x09;
    static constexpr std::int32_t kMaxLength = 256;

    std::string message;
    std::int64_t timestamp = 0;
    std::int64_t salt = 0;
    std::optional<MessageSignature> signature;
    LastSeenUpdate lastSeen;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChatMessagePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChatMessagePacket&, const ChatMessagePacket&) = default;
};

/// parity: `ArgumentSignatures.Entry`.
struct ArgumentSignature {
    static constexpr std::int32_t kMaxNameLength = 16;

    std::string name;
    MessageSignature signature{};

    friend bool operator==(const ArgumentSignature&, const ArgumentSignature&) = default;
};

struct SignedChatCommandPacket {
    static constexpr std::int32_t kId = 0x08;
    /// parity: `ArgumentSignatures.MAX_ARGUMENT_COUNT`.
    static constexpr std::int32_t kMaxArguments = 8;

    std::string command;
    std::int64_t timestamp = 0;
    std::int64_t salt = 0;
    std::vector<ArgumentSignature> argumentSignatures;
    LastSeenUpdate lastSeen;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SignedChatCommandPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SignedChatCommandPacket&, const SignedChatCommandPacket&) = default;
};

/// Serverbound, id 0x0a: the client's chat session, sent once it has a profile key.
struct ChatSessionUpdatePacket {
    static constexpr std::int32_t kId = 0x0a;

    RemoteChatSessionData session;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChatSessionUpdatePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChatSessionUpdatePacket&, const ChatSessionUpdatePacket&) = default;
};

/// parity: `MessageSignature.Packed`: an index into the receiver's signature cache, or the whole.
struct PackedSignature {
    /// parity: `Packed.FULL_SIGNATURE`.
    static constexpr std::int32_t kFullSignature = -1;

    std::int32_t id = kFullSignature;
    std::optional<MessageSignature> full;

    friend bool operator==(const PackedSignature&, const PackedSignature&) = default;
};

/// parity: `FilterMask`.
struct FilterMask {
    enum class Type : std::uint8_t { PassThrough, FullyFiltered, PartiallyFiltered };

    Type type = Type::PassThrough;
    std::vector<std::uint8_t> mask;

    friend bool operator==(const FilterMask&, const FilterMask&) = default;
};

struct ChatTypeBound {
    /// The index into the `minecraft:chat_type` registry the client was sent.
    std::int32_t chatType = 0;
    text::Component name;
    std::optional<text::Component> targetName;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ChatTypeBound> read(io::ByteBuffer& buffer);

    friend bool operator==(const ChatTypeBound&, const ChatTypeBound&) = default;
};

/// Clientbound, id 0x42: a player's message, with whatever proof of it the server holds.
struct PlayerChatPacket {
    static constexpr std::int32_t kId = 0x42;

    std::int32_t globalIndex = 0;
    common::Uuid sender;
    std::int32_t index = 0;
    std::optional<MessageSignature> signature;
    std::string content;
    std::int64_t timestamp = 0;
    std::int64_t salt = 0;
    std::vector<PackedSignature> lastSeen;
    std::optional<text::Component> unsignedContent;
    FilterMask filterMask;
    ChatTypeBound chatType;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PlayerChatPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PlayerChatPacket&, const PlayerChatPacket&) = default;
};

struct DisguisedChatPacket {
    static constexpr std::int32_t kId = 0x21;

    text::Component message;
    ChatTypeBound chatType;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<DisguisedChatPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const DisguisedChatPacket&, const DisguisedChatPacket&) = default;
};

} // namespace basalt::wire::protocol::play
