#pragma once

#include <basalt/wire/auth/SignatureValidator.hpp>
#include <basalt/wire/chat/ChatMessage.hpp>
#include <basalt/wire/chat/LastSeenValidator.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::chat {

/// parity: the chat half of `ServerGamePacketListenerImpl`.
class SecureChat {
public:
    /// parity: the pending count past which `sendPlayerChatMessage` disconnects.
    static constexpr std::size_t kMaxPendingChats = 4096;

    enum class SessionResult : std::uint8_t {
        kAccepted,
        kUnchanged,
        /// parity: "Ignoring chat session ... due to missing Services public key".
        kIgnored,
        /// parity: `EXPIRED_PROFILE_PUBLIC_KEY`, a key older than the one already in use.
        kExpiredKey,
        /// parity: `ProfilePublicKey.INVALID_SIGNATURE`.
        kInvalidSignature,
    };

    SecureChat(common::Uuid profileId, bool enforceSecureProfile, const auth::ServicesKeySet* services);

    SecureChat(const SecureChat&) = delete;
    SecureChat& operator=(const SecureChat&) = delete;
    SecureChat(SecureChat&&) = delete;
    SecureChat& operator=(SecureChat&&) = delete;
    ~SecureChat();

    /// parity: `handleChatSessionUpdate` and `resetPlayerChatState`.
    [[nodiscard]] SessionResult updateSession(const protocol::play::RemoteChatSessionData& data);

    [[nodiscard]] const std::optional<protocol::play::RemoteChatSessionData>& session() const noexcept {
        return session_;
    }

    [[nodiscard]] bool enforcesSecureProfile() const noexcept { return enforceSecureProfile_; }

    /// parity: `unpackAndApplyLastSeen`.
    [[nodiscard]] std::expected<std::vector<MessageSignature>, std::string>
    applyLastSeen(const protocol::play::LastSeenUpdate& update);

    /// parity: `handleChatAck`.
    [[nodiscard]] std::expected<void, std::string> applyAck(std::int32_t offset);

    /// parity: `SignedMessageChain.Decoder.unpack`.
    [[nodiscard]] std::expected<PlayerChatMessage, std::string> unpack(const std::optional<MessageSignature>& signature,
                                                                       SignedBody body, std::int64_t now);

    /// A relay cannot parse a command, so it counts its signatures into the chain without checking them.
    void skipSigned(std::size_t count, std::int64_t timestamp) noexcept;

    /// parity: `sendPlayerChatMessage` bookkeeping only, for a relay that sees the packet but does not write it.
    [[nodiscard]] bool noteDelivered(const MessageSignature& signature);

    [[nodiscard]] bool hasSession() const noexcept { return session_.has_value(); }

    /// parity: `Decoder.setChainBroken`.
    void breakChain() noexcept { nextIndex_.reset(); }

    /// parity: `sendPlayerChatMessage`.
    [[nodiscard]] bool send(const PlayerChatMessage& message, const BoundChatType& chatType,
                            const std::function<void(const protocol::play::PlayerChatPacket&)>& write);

private:
    common::Uuid profileId_;
    bool enforceSecureProfile_;
    const auth::ServicesKeySet* services_;

    std::optional<protocol::play::RemoteChatSessionData> session_;
    std::optional<auth::RsaPublicKey> key_;
    /// parity: `SignedMessageChain.nextLink`, empty once the chain is broken.
    std::optional<std::int32_t> nextIndex_;
    std::int64_t lastTimestamp_ = 0;

    std::mutex outgoingMutex_;
    LastSeenValidator lastSeen_;
    MessageSignatureCache cache_;
    std::int32_t nextChatIndex_ = 0;
};

[[nodiscard]] std::optional<std::string> disconnectKeyOf(SecureChat::SessionResult result);

} // namespace basalt::wire::chat
