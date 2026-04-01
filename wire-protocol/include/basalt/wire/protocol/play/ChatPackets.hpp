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

/// Secure chat: the chat session, signed messages and commands, their acknowledgements, and the
/// two ways a server relays a message.
namespace basalt::wire::protocol::play {

/// parity: `MessageSignature.BYTES`, an RSA-2048 signature, always this long on the wire.
inline constexpr std::size_t kMessageSignatureBytes = 256;

using MessageSignature = std::array<std::uint8_t, kMessageSignatureBytes>;

/// parity: `LastSeenMessages.Update`, the acknowledgement window a signed message carries.
struct LastSeenUpdate {
    /// parity: `LastSeenMessages.LAST_SEEN_MESSAGES_MAX_LENGTH`.
    static constexpr std::size_t kWindow = 20;
    static constexpr std::size_t kBytes = (kWindow + 7) / 8;

    std::int32_t offset = 0;
    /// A `BitSet` written fixed-width, little-endian by bit as `BitSet.toByteArray` lays it out.
    std::array<std::uint8_t, kBytes> acknowledged{};
    /// Zero means the client asked for no check.
    std::int8_t checksum = 0;

    [[nodiscard]] bool isAcknowledged(std::size_t index) const noexcept {
        return index / 8 < kBytes && ((acknowledged[index / 8] >> (index % 8)) & 1) != 0;
    }

    void setAcknowledged(std::size_t index) noexcept {
        acknowledged[index / 8] = static_cast<std::uint8_t>(acknowledged[index / 8] | (1U << (index % 8)));
    }

    /// `BitSet.length()`: one past the highest bit set, which may lie past the window.
    [[nodiscard]] std::size_t bitLength() const noexcept;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static LastSeenUpdate read(io::ByteBuffer& buffer);

    friend bool operator==(const LastSeenUpdate&, const LastSeenUpdate&) = default;
};

/// parity: `RemoteChatSession.Data`: the session id and the profile key Mojang signed for it.
struct RemoteChatSessionData {
    /// parity: `ByteBufCodecs.MAX_PUBLIC_KEY_LENGTH` and `ProfilePublicKey.Data.MAX_KEY_SIGNATURE_SIZE`.
    static constexpr std::int32_t kMaxPublicKeyBytes = 512;
    static constexpr std::int32_t kMaxKeySignatureBytes = 4096;

    common::Uuid sessionId;
    /// Epoch milliseconds, `ByteBufCodecs.INSTANT`.
    std::int64_t expiresAt = 0;
    /// X.509 SubjectPublicKeyInfo DER.
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

/// parity: `ArgumentSignatures.Entry`, one signed argument of a command, by its node name.
struct ArgumentSignature {
    static constexpr std::int32_t kMaxNameLength = 16;

    std::string name;
    MessageSignature signature{};

    friend bool operator==(const ArgumentSignature&, const ArgumentSignature&) = default;
};

/// Serverbound, id 0x08: a command with message arguments, which the client signs one by one when it
/// has a chat session and sends with no signatures when it has none.
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

/// parity: `MessageSignature.Packed`: an index into the receiver's signature cache, or the whole
/// signature when the cache does not hold it.
struct PackedSignature {
    /// parity: `Packed.FULL_SIGNATURE`.
    static constexpr std::int32_t kFullSignature = -1;

    std::int32_t id = kFullSignature;
    std::optional<MessageSignature> full;

    friend bool operator==(const PackedSignature&, const PackedSignature&) = default;
};

/// parity: `FilterMask`, by its three types. Only a partial mask carries bits.
struct FilterMask {
    enum class Type : std::uint8_t { PassThrough, FullyFiltered, PartiallyFiltered };

    Type type = Type::PassThrough;
    /// `BitSet.toByteArray`, for a partial mask.
    std::vector<std::uint8_t> mask;

    friend bool operator==(const FilterMask&, const FilterMask&) = default;
};

/// parity: `ChatType.Bound`: which chat type, and the names its decoration is filled in with.
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

/// Clientbound, id 0x21: a message that looks like chat but carries no sender to verify, which is
/// what a command run by anything other than a signing player says.
struct DisguisedChatPacket {
    static constexpr std::int32_t kId = 0x21;

    text::Component message;
    ChatTypeBound chatType;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<DisguisedChatPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const DisguisedChatPacket&, const DisguisedChatPacket&) = default;
};

} // namespace basalt::wire::protocol::play
