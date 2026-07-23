#include <basalt/wire/chat/SecureChat.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace basalt::wire::chat {

SecureChat::SecureChat(common::Uuid profileId, bool enforceSecureProfile, const auth::ServicesKeySet* services)
    : profileId_(profileId), enforceSecureProfile_(enforceSecureProfile), services_(services) {}

SecureChat::~SecureChat() = default;

SecureChat::SessionResult SecureChat::updateSession(const protocol::play::RemoteChatSessionData& data) {
    const bool sameKey = session_.has_value() && session_->expiresAt == data.expiresAt &&
                         session_->publicKey == data.publicKey && session_->keySignature == data.keySignature;
    if (sameKey) {
        return SessionResult::Unchanged;
    }
    if (session_.has_value() && data.expiresAt < session_->expiresAt) {
        return SessionResult::ExpiredKey;
    }
    if (services_ == nullptr || !services_->canValidateProfileKeys()) {
        return SessionResult::Ignored;
    }
    // parity: a key that does not parse fails the packet in vanilla; either way the player goes.
    std::optional<auth::RsaPublicKey> key = auth::RsaPublicKey::parseDer(data.publicKey);
    if (!key.has_value() ||
        !services_->verifyProfileKey(auth::profileKeyPayload(profileId_, data.expiresAt, data.publicKey),
                                     data.keySignature)) {
        return SessionResult::InvalidSignature;
    }
    session_ = data;
    key_ = std::move(key);
    nextIndex_ = 0;
    lastTimestamp_ = 0;
    return SessionResult::Accepted;
}

std::expected<std::vector<MessageSignature>, std::string>
SecureChat::applyLastSeen(const protocol::play::LastSeenUpdate& update) {
    const std::scoped_lock lock{outgoingMutex_};
    return lastSeen_.applyUpdate(update);
}

std::expected<void, std::string> SecureChat::applyAck(std::int32_t offset) {
    const std::scoped_lock lock{outgoingMutex_};
    return lastSeen_.applyOffset(offset);
}

std::expected<PlayerChatMessage, std::string> SecureChat::unpack(const std::optional<MessageSignature>& signature,
                                                                 SignedBody body, std::int64_t now) {
    if (!session_.has_value()) {
        // parity: `Decoder.unsigned`: what was signed is dropped, and the message is stamped now.
        if (enforceSecureProfile_) {
            return std::unexpected{std::string{"chat.disabled.missingProfileKey"}};
        }
        return PlayerChatMessage::unsignedFrom(profileId_, std::move(body.content), now);
    }
    if (!signature.has_value()) {
        return std::unexpected{std::string{"chat.disabled.missingProfileKey"}};
    }
    if (session_->expiresAt < now) {
        return std::unexpected{std::string{"chat.disabled.expiredProfileKey"}};
    }
    if (!nextIndex_.has_value()) {
        return std::unexpected{std::string{"chat.disabled.chain_broken"}};
    }
    if (body.timestamp < lastTimestamp_) {
        breakChain();
        return std::unexpected{std::string{"chat.disabled.out_of_order_chat"}};
    }
    lastTimestamp_ = body.timestamp;
    PlayerChatMessage message;
    message.sender = profileId_;
    message.sessionId = session_->sessionId;
    message.index = *nextIndex_;
    message.signature = signature;
    message.body = std::move(body);
    if (!key_->verify(auth::SignatureAlgorithm::Sha256WithRsa, message.signedPayload(), *signature)) {
        breakChain();
        return std::unexpected{std::string{"chat.disabled.invalid_signature"}};
    }
    // parity: `SignedMessageLink.advance`, which ends the chain rather than wrapping.
    nextIndex_ =
        *nextIndex_ == std::numeric_limits<std::int32_t>::max() ? std::nullopt : std::optional{*nextIndex_ + 1};
    return message;
}

void SecureChat::skipSigned(std::size_t count, std::int64_t timestamp) noexcept {
    if (!nextIndex_.has_value() || count == 0) {
        return;
    }
    lastTimestamp_ = std::max(lastTimestamp_, timestamp);
    const std::int64_t next = static_cast<std::int64_t>(*nextIndex_) + static_cast<std::int64_t>(count);
    nextIndex_ =
        next > std::numeric_limits<std::int32_t>::max() ? std::nullopt : std::optional{static_cast<std::int32_t>(next)};
}

bool SecureChat::noteDelivered(const MessageSignature& signature) {
    const std::scoped_lock lock{outgoingMutex_};
    lastSeen_.addPending(signature);
    return lastSeen_.trackedCount() <= kMaxPendingChats;
}

bool SecureChat::send(const PlayerChatMessage& message, const BoundChatType& chatType,
                      const std::function<void(const protocol::play::PlayerChatPacket&)>& write) {
    const std::scoped_lock lock{outgoingMutex_};
    protocol::play::PlayerChatPacket packet;
    packet.globalIndex = nextChatIndex_++;
    packet.sender = message.sender;
    packet.index = message.index;
    packet.signature = message.signature;
    packet.content = message.body.content;
    packet.timestamp = message.body.timestamp;
    packet.salt = message.body.salt;
    for (const MessageSignature& seen : message.body.lastSeen) {
        const std::int32_t id = cache_.pack(seen);
        packet.lastSeen.push_back(id >= 0 ? protocol::play::PackedSignature{.id = id, .full = std::nullopt}
                                          : protocol::play::PackedSignature{
                                                .id = protocol::play::PackedSignature::kFullSignature, .full = seen});
    }
    packet.unsignedContent = message.unsignedContent;
    packet.chatType = chatType.toWire();
    write(packet);
    if (!message.signature.has_value()) {
        return true;
    }
    cache_.push(message.body.lastSeen, message.signature);
    lastSeen_.addPending(*message.signature);
    return lastSeen_.trackedCount() <= kMaxPendingChats;
}

std::optional<std::string> disconnectKeyOf(SecureChat::SessionResult result) {
    switch (result) {
    case SecureChat::SessionResult::ExpiredKey:
        return std::string{"multiplayer.disconnect.expired_public_key"};
    case SecureChat::SessionResult::InvalidSignature:
        return std::string{"multiplayer.disconnect.invalid_public_key_signature"};
    default:
        return std::nullopt;
    }
}

} // namespace basalt::wire::chat
